#ifndef NO_JIT_COMPILER
#include <ogc/cache.h>
#include "JIT.h"

JITCache jitCache;

alignas(32) u8 smcPageFlags[SMC_MAP_SIZE] = {0};
BasicBlock* smcRegistry[SMC_MAP_SIZE] = {nullptr};

JITCache::JITCache() {
	memset(blockTable, 0, sizeof(blockTable));
	arenaOffset = 0;
	jitArena = (u32*)memalign(32, JIT_ARENA_SIZE);
	flushCache();
}

JITCache::~JITCache() {
	flushCache();
	if (jitArena) {
		free(jitArena);
		jitArena = nullptr;
	}
}

u32* JITCache::allocateJITMemory(size_t numBytes) {
	numBytes = (numBytes + 31) & ~31;

	// If this allocation exceeds our 512KB arena, flush the cache
	if (arenaOffset + numBytes > JIT_ARENA_SIZE) {
		flushCache();
	}

	// Grab the current pointer and bump the offset
	u32* allocatedPtr = (u32*)((u8*)jitArena + arenaOffset);
	arenaOffset += numBytes;
	return allocatedPtr;
}

void JITCache::rewindJITMemory(size_t numBytes) {
	// The caller strictly calculates a 32-byte aligned rewind size
	// Wind back the offset to recover unused memory (e.g., from bailing out)
	if (arenaOffset >= numBytes) {
		arenaOffset -= numBytes;
	} else {
		arenaOffset = 0; // Fallback underflow protection
	}
}

BasicBlock* JITCache::registerBlock(u32 pc, u32 length, JITBlockFunc execute) {
	u32 index = ((pc >> 1) ^ (pc >> 13)) & (HASH_TABLE_SIZE - 1);

	u32 evictedPC = blockTable[index].startPC;
	PROFILER_CACHE_EVICT(evictedPC, pc);

	BasicBlock* block = &blockTable[index];

	// Unlink evicted block from the SMC bucket to prevent dangling pointers
	if (block->execute != nullptr && block->length > 0) {
		u8 oldBank = (evictedPC >> 24) & 0xFF;
		if (oldBank == 2 || oldBank == 3) { // EWRAM or IWRAM
			u32 oldPage = (evictedPC >> 10) & 0xFFFF;
			BasicBlock* curr = smcRegistry[oldPage];
			BasicBlock* prev = nullptr;

			while (curr) {
				if (curr == block) {
					if (prev) {
						prev->nextSMC = curr->nextSMC;
					} else {
						smcRegistry[oldPage] = curr->nextSMC;
					}
					break;
				}
				prev = curr;
				curr = curr->nextSMC;
			}

			// If the bucket is now empty, clear the global page flag
			if (smcRegistry[oldPage] == nullptr) {
				smcPageFlags[oldPage] = 0;
			}
		}
	}

	block->startPC = pc;
	block->length = length;
	block->execute = execute;
	block->nextSMC = nullptr;

	// Register with SMC tracker
	if (execute != nullptr && length > 0) {
		u8 bank = (pc >> 24) & 0xFF;
		if (bank == 2 || bank == 3) { // EWRAM or IWRAM
			u32 startPage = (pc >> 10) & 0xFFFF;
			smcPageFlags[startPage] = 1;
			// Intrusively push block to registry head
			block->nextSMC = smcRegistry[startPage];
			smcRegistry[startPage] = block;
		}
	}

	JIT_LOG_CACHE_EVENT(index, pc, evictedPC, arenaOffset, arenaOffset);
	return block;
}

void JITCache::flushCache() {
	PROFILER_CACHE_FLUSH_START();
	JIT_LOG_CACHE_FLUSH();

	arenaOffset = 0;
	memset(blockTable, 0, sizeof(blockTable));
	memset(smcPageFlags, 0, sizeof(smcPageFlags));
	memset(smcRegistry, 0, sizeof(smcRegistry));

	if (jitArena) {
		u32* emitPtr = jitArena;
		linkerStubAddress = emitPtr;

		// 1. Load the Base Address of the blockTable struct array
		*emitPtr++ = PPC_LIS(PPC_R10, (u32)&blockTable[0] >> 16);
		*emitPtr++ = PPC_ORI(PPC_R10, PPC_R10, (u32)&blockTable[0] & 0xFFFF);

		// 2. Native Hash Calculation
		*emitPtr++ = PPC_SRWI(PPC_R11, PPC_R4, 1);
		*emitPtr++ = PPC_SRWI(PPC_R12, PPC_R4, 13);
		*emitPtr++ = PPC_XOR(PPC_R11, PPC_R11, PPC_R12);
		// Dynamically calculate the Native Mask boundaries based on HASH_TABLE_SIZE
		u32 hashBits = __builtin_ctz(HASH_TABLE_SIZE); // 8192 = 13, 32768 = 15, 65536 = 16
		u32 maskBegin = 27 - hashBits + 1;

		*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 4, maskBegin, 27);
		*emitPtr++ = PPC_ADD(PPC_R11, PPC_R10, PPC_R11);

		// 3. Miss Guard 1: PC Collision Check
		*emitPtr++ = PPC_LWZ(PPC_R12, PPC_R11, 0);
		*emitPtr++ = PPC_CMPW(0, PPC_R12, PPC_R4);
		u32* branchCollision = emitPtr;
		*emitPtr++ = PPC_BNE(0);

		// 4. CACHE HIT: Extract Block execution address (execute is at offset 8)
		*emitPtr++ = PPC_LWZ(PPC_R12, PPC_R11, 8);

		// 5. Miss Guard 2: Hard Stop for Fallback Blocks (execute == nullptr)
		*emitPtr++ = PPC_CMPWI(0, PPC_R12, 0);
		u32* branchFailBlock = emitPtr;
		*emitPtr++ = PPC_BEQ(0);

		// 6. DIRECT PATCHING (Only runs for valid, executable JIT blocks!)
		*emitPtr++ = PPC_MFLR(PPC_R10);
		*emitPtr++ = PPC_ADDI(PPC_R10, PPC_R10, -4);
		*emitPtr++ = PPC_SUBF(PPC_R11, PPC_R10, PPC_R12);
		*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 0, 6, 29);
		*emitPtr++ = PPC_ORIS(PPC_R11, PPC_R11, 0x4800);
		*emitPtr++ = PPC_STW(PPC_R11, PPC_R10, 0);

		// 7. Flush Broadway CPU Cache
		*emitPtr++ = PPC_DCBST(0, PPC_R10);
		*emitPtr++ = PPC_SYNC();
		*emitPtr++ = PPC_ICBI(0, PPC_R10);
		*emitPtr++ = PPC_SYNC();
		*emitPtr++ = PPC_ISYNC();

		// 8. Execute Target Block
		*emitPtr++ = PPC_MTCTR(PPC_R12);
		*emitPtr++ = PPC_BCTR();

		// 9. CACHE MISS / FALLBACK YIELD TARGET
		u32* missTarget = emitPtr;
		linkerReturnAddress = missTarget;

		// Correctly route code collisions and fail blocks out to the C++ handler
		*branchCollision = PPC_BNE((u32)((missTarget - branchCollision) * 4));
		*branchFailBlock = PPC_BEQ((u32)((missTarget - branchFailBlock) * 4));

		*emitPtr++ = PPC_LIS(PPC_R12, (u32)&ExecuteJITTrace_Return >> 16);
		*emitPtr++ = PPC_ORI(PPC_R12, PPC_R12, (u32)&ExecuteJITTrace_Return & 0xFFFF);
		*emitPtr++ = PPC_MTCTR(PPC_R12);
		*emitPtr++ = PPC_BCTR();

		arenaOffset = ((emitPtr - jitArena) * sizeof(u32) + 31) & ~31;

		// Flush the newly generated Linker Stub from D-Cache to I-Cache
		u32 stubSize = (u8*)emitPtr - (u8*)jitArena;
		DCStoreRange((void*)jitArena, stubSize);
		ICInvalidateRange((void*)jitArena, stubSize);
	}
	PROFILER_CACHE_FLUSH_END();
}

// SMC eviction handler
void JITCache::invalidateSMCTarget(u32 targetEA) {
	// Maximum trace length is (JIT_TRACE_MAX_INSTRUCTIONS+1)*2 bytes. Maximum write size is 36 bytes.
	// Branchless minimum boundary clamping at 0
	s32 offsetDiff = (s32)(targetEA - ((JIT_TRACE_MAX_INSTRUCTIONS + 1) * 2));
	u32 startEA = offsetDiff & ~(offsetDiff >> 31);
	u32 endEA = targetEA + 36;

	u32 startPage = (startEA >> 10) & 0xFFFF;
	u32 endPage = (endEA >> 10) & 0xFFFF;

	for (u32 page = startPage; page <= endPage; page++) {
		BasicBlock* curr = smcRegistry[page];
		BasicBlock* prev = nullptr;

		while (curr) {
			u32 blockStartPC = curr->startPC;
			u32 blockEndPC = blockStartPC + (curr->length * 2);

			// Overlap Detection: Write Range vs Block Range
			if (targetEA < blockEndPC && endEA > blockStartPC) {
				if (curr->execute) {
					// Surgical Trampoline Patch: Overwrite first instruction with PPC_B to exit handler
					u32* codePtr = (u32*)curr->execute;
					s32 branchOffset = (s32)((u8*)linkerReturnAddress - (u8*)codePtr);
					*codePtr = PPC_B(branchOffset);

					// Hardware Cache Sync on 4-byte patched instruction
					DCStoreRange(codePtr, 4);
					ICInvalidateRange(codePtr, 4);
					curr->execute = nullptr; // Mark execute as null to force cache miss on next lookup
				}

				// Remove block from the SMC bucket linked list
				BasicBlock* next = curr->nextSMC;
				if (prev) {
					prev->nextSMC = next;
				} else {
					smcRegistry[page] = next;
				}
				curr = next;
			} else {
				prev = curr;
				curr = curr->nextSMC;
			}
		}

		// Clear smcPageFlags byte if bucket list is now completely empty
		if (smcRegistry[page] == nullptr) {
			smcPageFlags[page] = 0;
		}
	}
}

#endif

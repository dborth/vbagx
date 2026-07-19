#ifndef NO_JIT_COMPILER
#include "JITCache.h"

JITCache jitCache;

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
	// Round DOWN the reclaimed bytes so we don't pull the arena offset
	// backward into the instructions we actually emitted!
	numBytes = numBytes & ~31;

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

    blockTable[index].startPC = pc;
    blockTable[index].length = length;
    blockTable[index].execute = execute;

    JIT_LOG_CACHE_EVENT(index, pc, evictedPC, arenaOffset, arenaOffset);
    return &blockTable[index];
}

void JITCache::flushCache() {
	JIT_LOG_CACHE_FLUSH();

    arenaOffset = 0;
    memset(blockTable, 0, sizeof(blockTable));

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
		*emitPtr++ = PPC_LIS(PPC_R0, 0x4800);
		*emitPtr++ = PPC_OR(PPC_R11, PPC_R11, PPC_R0);
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
        u32 start = (u32)jitArena & ~31;
        u32 end   = ((u32)emitPtr + 31) & ~31;
        for (u32 i = start; i < end; i += 32) asm volatile("dcbst 0, %0" : : "r" (i) : "memory");
        asm volatile("sync" : : : "memory");
        for (u32 i = start; i < end; i += 32) asm volatile("icbi 0, %0" : : "r" (i) : "memory");
        asm volatile("sync \n isync" : : : "memory");
    }
}
#endif

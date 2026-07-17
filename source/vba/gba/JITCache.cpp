#include "JITCache.h"
#include "JITDebug.h"
#include "JITPPCEmitter.h"

#include <string.h>

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
    numBytes = (numBytes + 3) & ~3;
    if (arenaOffset + (numBytes / sizeof(u32)) >= (JIT_ARENA_SIZE / sizeof(u32))) {
        flushCache();
    }
    u32* ptr = &jitArena[arenaOffset];
    arenaOffset += (numBytes / sizeof(u32));
    return ptr;
}

void JITCache::rewindJITMemory(size_t numBytes) {
    size_t words = (numBytes + 3) / sizeof(u32);
    if (arenaOffset >= words) arenaOffset -= words;
    else arenaOffset = 0;
}

void JITCache::registerBlock(BasicBlock* block) {
    if (!block) return;
    u32 index = ((block->startPC >> 1) ^ (block->startPC >> 13)) & (HASH_TABLE_SIZE - 1);

	u32 evictedPC = 0;
	if (blockTable[index]) {
		evictedPC = blockTable[index]->startPC;
		delete blockTable[index];
	}
	blockTable[index] = block;
    JIT_LOG_CACHE_EVENT(index, block->startPC, evictedPC, arenaOffset, arenaOffset);
}

void JITCache::flushCache() {
	JIT_LOG_CACHE_FLUSH();

    arenaOffset = 0;
    for (size_t i = 0; i < HASH_TABLE_SIZE; ++i) {
        if (blockTable[i]) {
            delete blockTable[i];
            blockTable[i] = nullptr;
        }
    }

    if (jitArena) {
        u32* emitPtr = jitArena;
        linkerStubAddress = emitPtr;

        // 1. Load the C++ blockTable Base Address
        *emitPtr++ = PPC_LIS(PPC_R10, (u32)blockTable >> 16);
        *emitPtr++ = PPC_ORI(PPC_R10, PPC_R10, (u32)blockTable & 0xFFFF);

        // 2. Native Hash: index = ((R4 >> 1) ^ (R4 >> 13)) & 8191
        *emitPtr++ = PPC_SRWI(PPC_R11, PPC_R4, 1);
        *emitPtr++ = PPC_SRWI(PPC_R12, PPC_R4, 13);
        *emitPtr++ = PPC_XOR(PPC_R11, PPC_R11, PPC_R12);
        *emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 17, 29);

        // 3. Fetch BasicBlock*
        *emitPtr++ = PPC_LWZX(PPC_R11, PPC_R10, PPC_R11);

        // 4. Miss Guard 1: NULL Check
        *emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
        u32* branchNull = emitPtr;
        *emitPtr++ = PPC_BEQ(0);

        // 5. Miss Guard 2: PC Collision Check
		*emitPtr++ = PPC_LWZ(PPC_R12, PPC_R11, 0); // block->startPC
		*emitPtr++ = PPC_CMPW(0, PPC_R12, PPC_R4);
		u32* branchCollision = emitPtr;
		*emitPtr++ = PPC_BNE(0);

		// 6. CACHE HIT: Extract Block execution address
		*emitPtr++ = PPC_LWZ(PPC_R12, PPC_R11, 8); // block->execute

		// BUGFIX: Guard against "Fail Blocks" (execute == nullptr). Treat as a Cache Miss.
		*emitPtr++ = PPC_CMPWI(0, PPC_R12, 0);
		u32* branchFailBlock = emitPtr;
		*emitPtr++ = PPC_BEQ(0);

		// 7. DIRECT PATCHING: Find the instruction that called us (via LR)
		*emitPtr++ = PPC_MFLR(PPC_R10);
		*emitPtr++ = PPC_ADDI(PPC_R10, PPC_R10, -4); // R10 = Patch Site Address

		// 8. Calculate relative branch offset (Target - Patch Site)
		*emitPtr++ = PPC_SUBF(PPC_R11, PPC_R10, PPC_R12); // R11 = R12 - R10
		*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 0, 6, 29); // Mask to 26 bits

		// 9. Construct 'b offset' native instruction (0x48000000)
		*emitPtr++ = PPC_LIS(PPC_R0, 0x4800);
		*emitPtr++ = PPC_OR(PPC_R11, PPC_R11, PPC_R0);

		// 10. Write patched instruction into the caller's block
		*emitPtr++ = PPC_STW(PPC_R11, PPC_R10, 0);

		// 11. Flush Broadway CPU Cache to make the patch executable
		*emitPtr++ = PPC_DCBST(0, PPC_R10);
		*emitPtr++ = PPC_SYNC();
		*emitPtr++ = PPC_ICBI(0, PPC_R10);
		*emitPtr++ = PPC_SYNC();
		*emitPtr++ = PPC_ISYNC();

		// 12. Execute Target Block
		*emitPtr++ = PPC_MTCTR(PPC_R12);
		*emitPtr++ = PPC_BCTR();

		// 13. CACHE MISS / UNIVERSAL YIELD TARGET
		u32* missTarget = emitPtr;
		linkerReturnAddress = missTarget; // Lock this globally for all JIT bailouts

		*branchNull = PPC_BEQ((u32)((missTarget - branchNull) * 4));
		*branchCollision = PPC_BNE((u32)((missTarget - branchCollision) * 4));
		*branchFailBlock = PPC_BEQ((u32)((missTarget - branchFailBlock) * 4)); // Resolve Null Guard

		*emitPtr++ = PPC_LIS(PPC_R12, (u32)&ExecuteJITTrace_Return >> 16);
		*emitPtr++ = PPC_ORI(PPC_R12, PPC_R12, (u32)&ExecuteJITTrace_Return & 0xFFFF);
		*emitPtr++ = PPC_MTCTR(PPC_R12);
		*emitPtr++ = PPC_BCTR();

        arenaOffset = (emitPtr - jitArena);

        // Flush the newly generated Linker Stub from D-Cache to I-Cache
        u32 start = (u32)jitArena & ~31;
        u32 end   = ((u32)emitPtr + 31) & ~31;
        for (u32 i = start; i < end; i += 32) asm volatile("dcbst 0, %0" : : "r" (i) : "memory");
        asm volatile("sync" : : : "memory");
        for (u32 i = start; i < end; i += 32) asm volatile("icbi 0, %0" : : "r" (i) : "memory");
        asm volatile("sync \n isync" : : : "memory");
    }
}

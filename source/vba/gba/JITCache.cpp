#include "JITCache.h"
#include "JITDebug.h"

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
        u32 start = (u32)jitArena & ~31;
        u32 end   = ((u32)jitArena + JIT_ARENA_SIZE + 31) & ~31;
        for (u32 i = start; i < end; i += 32) {
            asm volatile("dcbst 0, %0" : : "r" (i) : "memory");
        }
        asm volatile("sync" : : : "memory");

        for (u32 i = start; i < end; i += 32) {
            asm volatile("icbi 0, %0" : : "r" (i) : "memory");
        }
        asm volatile("sync \n isync" : : : "memory");
    }
}

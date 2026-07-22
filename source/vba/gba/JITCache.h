#ifndef JIT_CACHE_H
#define JIT_CACHE_H

#ifndef NO_JIT_COMPILER
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "../common/Port.h"
#include "JITPPCEmitter.h"
#include "JITDebug.h"

#define JIT_ARENA_SIZE					(1024 * 1024 * 4) // 4 MB
#define HASH_TABLE_SIZE					65536

// -------------------------------------------------------------------------
// ENGINE DEFINITIONS
// -------------------------------------------------------------------------
typedef void (*JITBlockFunc)();

// Force 16-byte alignment to allow fast PowerPC bit-shifting
struct __attribute__((aligned(16))) BasicBlock {
    u32 startPC;
    u32 length;
    JITBlockFunc execute;
    u32 padding;
};

class JITCache {
	private:
		u32* jitArena;
		size_t arenaOffset;

		alignas(32) BasicBlock blockTable[HASH_TABLE_SIZE];

	public:
		JITCache();
		~JITCache();

		u32* linkerStubAddress;
		u32* linkerReturnAddress;

		u32* allocateJITMemory(size_t numBytes);
		void rewindJITMemory(size_t numBytes);
        BasicBlock* registerBlock(u32 pc, u32 length, JITBlockFunc execute);
        inline size_t getArenaOffset() const { return arenaOffset; }
		void flushCache();

		inline BasicBlock* getBlock(u32 pc) {
			u32 index = ((pc >> 1) ^ (pc >> 13)) & (HASH_TABLE_SIZE - 1);
			BasicBlock* block = &blockTable[index];
			if (block->startPC == pc) {
				PROFILER_CACHE_HIT();
				return block;
			}
			PROFILER_CACHE_MISS();
			return nullptr;
		}

#if JIT_BLOCK_FRAGMENTATION_STATS
		inline bool isMidBlockRecompile(u32 pc) const {
			for (u32 i = 0; i < HASH_TABLE_SIZE; i++) {
				const BasicBlock& b = blockTable[i];
				if (b.execute != nullptr && b.startPC != pc) {
					u32 endPC = b.startPC + (b.length * 2);
					if (pc > b.startPC && pc < endPC) {
						return true;
					}
				}
			}
			return false;
		}
#endif
};

#endif
#endif // JIT_CACHE_H

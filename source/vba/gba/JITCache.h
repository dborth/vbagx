#ifndef JIT_CACHE_H
#define JIT_CACHE_H

#ifndef NO_JIT_COMPILER
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "../common/Port.h"
#include "JITPPCEmitter.h"
#include "JITDebug.h"

#define JIT_ARENA_SIZE					(1024 * 1024 * 8) // 8 MB
#define HASH_TABLE_SIZE					65536
#define SMC_MAP_SIZE                    65536 // 64K pages (1KB page granularity across 64MB)

// -------------------------------------------------------------------------
// ENGINE DEFINITIONS
// -------------------------------------------------------------------------
typedef void (*JITBlockFunc)();

// Force 16-byte alignment to allow fast PowerPC bit-shifting
struct __attribute__((aligned(16))) BasicBlock {
    u32 startPC;
    u32 length;
    JITBlockFunc execute;
    BasicBlock* nextSMC; // Intrusive pointer for SMC bucket chain
};

// Global SMC tracking structures
extern u8 smcPageFlags[SMC_MAP_SIZE];
extern BasicBlock* smcRegistry[SMC_MAP_SIZE];

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
        void invalidateSMCTarget(u32 targetEA);

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
};

#endif
#endif // JIT_CACHE_H

#ifndef JIT_CACHE_H
#define JIT_CACHE_H

#include "JITDebug.h"

#ifndef NO_JIT_COMPILER
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "../common/Port.h"

extern "C" void ExecuteJITTrace_Return();

// -------------------------------------------------------------------------
// ENGINE DEFINITIONS
// -------------------------------------------------------------------------
typedef void (*JITBlockFunc)();

struct JITResult {
    u32 cycles;
    u32 nextPC;
};

// Force 16-byte alignment to allow fast PowerPC bit-shifting
struct __attribute__((aligned(16))) BasicBlock {
    u32 startPC;
    u32 length;
    JITBlockFunc execute;
    u32 padding;
};

class JITCache {
	private:
		static constexpr size_t JIT_ARENA_SIZE = 512 * 1024; // 512 KB
		u32* jitArena;
		size_t arenaOffset;

		static constexpr size_t HASH_TABLE_SIZE = 8192;
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
				return block;
			}
			return nullptr;
		}
};

extern JITCache jitCache;

extern "C" void ExecuteJITTrace(JITBlockFunc execute, u32* gbaRegs, u32* flags, JITResult* outResult, u8** readPages, u32* readMasks, u32* busPrefetchCount);

BasicBlock* JITCompileThumbTrace(u32 startPC, JITCache& cache);
#endif
#endif // JIT_CACHE_H

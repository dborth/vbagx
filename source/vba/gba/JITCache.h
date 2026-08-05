/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * JITCache.h
 *
 * Declares the JITCache class and its supporting data structures:
 *   - BasicBlock: the 16-byte-aligned {startPC, length, execute, nextSMC}
 *     record stored per hash bucket. `execute == nullptr` with `length == 0`
 *     represents an as-yet-uncompiled slot; `execute == nullptr` with a
 *     nonzero length represents a deliberately cached "don't JIT this"
 *     fallback stub (as opposed to a genuine cache miss); `nextSMC` is an
 *     intrusive linked-list pointer used only by the SMC registry.
 *   - smcPageFlags[] / smcRegistry[]: the global, 1KB-page-granularity
 *     self-modifying-code tracking tables. A set flag means "at least one
 *     compiled block currently has code living on this page"; the registry
 *     holds the actual intrusive per-page block lists that invalidateSMCTarget()
 *     walks on every guest write to EWRAM/IWRAM.
 *   - JITCache: owns the arena pointer/offset, the block hash table, and
 *     the linker stub addresses (linkerStubAddress/linkerReturnAddress)
 *     that JIT-emitted code branches to directly. getBlock() is the hot-path
 *     inline lookup the interpreter's dispatch loop calls on every THUMB
 *     fetch; the heavier registerBlock()/flushCache()/invalidateSMCTarget()
 *     logic lives in JITCache.cpp.
 *
 * JIT_ARENA_SIZE (8MB), HASH_TABLE_SIZE (65536), and SMC_MAP_SIZE are the
 * settled tuning constants
 ***************************************************************************/

#ifndef JIT_CACHE_H
#define JIT_CACHE_H

#include <stddef.h>
#include <string.h>
#include "../common/Port.h"
#include "JITPPCEmitter.h"
#include "Debug.h"

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

class JITCache {
	private:
		u32* jitArena;
		size_t arenaOffset;
		BasicBlock* blockTable;
		BasicBlock** smcRegistry;
		bool isInitialized;

	public:
		JITCache();
		~JITCache();

		u32* linkerStubAddress;
		u32* linkerReturnAddress;
		u8* smcPageFlags;

		void initialize(u32* arenaPtr, BasicBlock* blockPtr, BasicBlock** smcRegPtr, u8* smcFlagsPtr);
		void destroy();

		u32* allocateJITMemory(size_t numBytes);
		void rewindJITMemory(size_t numBytes);
		BasicBlock* registerBlock(u32 pc, u32 length, JITBlockFunc execute);
		inline size_t getArenaOffset() const { return arenaOffset; }
		void flushCache();
		void invalidateSMCTarget(u32 targetEA);

		inline BasicBlock* getBlock(u32 pc) {
			if (!isInitialized) return nullptr;
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

extern JITCache jitCache;

#endif

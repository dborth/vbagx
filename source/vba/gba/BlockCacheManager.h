#ifndef BLOCK_CACHE_MANAGER_H
#define BLOCK_CACHE_MANAGER_H

#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include "../common/Port.h"

// -------------------------------------------------------------------------
// POWERPC BROADWAY EMITTER MACROS & REGISTER MAPPINGS
// -------------------------------------------------------------------------
#define PPC_R3   3
#define PPC_R4   4
#define PPC_R5   5
#define PPC_R6   6   // Dedicated C_FLAG register
#define PPC_R7   7   // Dedicated V_FLAG register
#define PPC_R8   8   // Dedicated N_FLAG register
#define PPC_R9   9   // Dedicated Z_FLAG register
#define PPC_R10  10  // Scratch: Base Page Pointer
#define PPC_R11  11  // Scratch: Bank / Mask / Condition
#define PPC_R12  12  // Scratch: Target Address / Operand

#define PPC_R14  14  // Base GBA Register (GBA R0 -> PPC R14 ... GBA R15 -> PPC R29)

#define PPC_R30_PAGES 30  // readPages Base Pointer Array (Non-volatile, no overlap with GBA registers)
#define PPC_R31_MASKS 31  // readMasks Base Pointer Array (Non-volatile, no overlap with GBA registers)

#define PPC_REG_C PPC_R6
#define PPC_REG_V PPC_R7
#define PPC_REG_N PPC_R8
#define PPC_REG_Z PPC_R9

// Map GBA R0..R15 cleanly to PowerPC R14..R29
inline int MapGBARegister(int gbaReg) { return PPC_R14 + gbaReg; }

// ALU Operations
#define PPC_ADD(rD, rA, rB)    ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (266 << 1))
#define PPC_SUBF(rD, rA, rB)   ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (40 << 1))
#define PPC_AND(rA, rS, rB)    ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (28 << 1))
#define PPC_OR(rA, rS, rB)     ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (444 << 1))

// Hardware Flag Math (XER & Zero Checks)
#define PPC_ADDCO(rD, rA, rB)  ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (1 << 10) | (10 << 1))
#define PPC_SUBFCO(rD, rA, rB) ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (1 << 10) | (8 << 1))
#define PPC_MFXER(rD)          ((31 << 26) | ((rD) << 21) | (1 << 16) | (339 << 1))
#define PPC_CNTLZW(rA, rS)     ((31 << 26) | ((rS) << 21) | ((rA) << 16) | (26 << 1))

// Immediate Operations
#define PPC_ADDI(rD, rA, val)  ((14 << 26) | ((rD) << 21) | ((rA) << 16) | ((val) & 0xFFFF))
#define PPC_LI(rD, val)        PPC_ADDI(rD, 0, val)
#define PPC_LIS(rD, val)       ((15 << 26) | ((rD) << 21) | (0 << 16) | ((val) & 0xFFFF))
#define PPC_ORI(rA, rS, val)   ((24 << 26) | ((rS) << 21) | ((rA) << 16) | ((val) & 0xFFFF))

// Branches
#define PPC_CMPWI(cr, rA, imm) ((11 << 26) | ((cr) << 23) | ((rA) << 16) | ((imm) & 0xFFFF))
#define PPC_BNE(offset)        ((16 << 26) | (4 << 21) | (2 << 16) | ((offset) & 0xFFFC))
#define PPC_BEQ(offset)        ((16 << 26) | (12 << 21) | (2 << 16) | ((offset) & 0xFFFC))
#define PPC_BGE(offset)        ((16 << 26) | (4 << 21) | (0 << 16) | ((offset) & 0xFFFC))
#define PPC_B(offset)          ((18 << 26) | ((offset) & 0x3FFFFFC))
#define PPC_BLR()              0x4E800020

// Advanced Memory & Bitwise
// Endian-Correct Memory Loads & Stores (GBA is Little-Endian, Wii is Big-Endian)
#define PPC_LWZX(rD, rA, rB)   ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (23 << 1))
#define PPC_LWBRX(rD, rA, rB)			((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (534 << 1)) // Load Word Byte-Reverse
#define PPC_LHBRX(rD, rA, rB)			((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (790 << 1)) // Load Halfword Byte-Reverse
#define PPC_LBZX(rD, rA, rB)			((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (87 << 1))  // Load Byte Zero

#define PPC_STWBRX(rS, rA, rB)          ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (662 << 1)) // Store Word Byte-Reverse
#define PPC_STHBRX(rS, rA, rB)          ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (918 << 1)) // Store Halfword Byte-Reverse
#define PPC_STBZX(rS, rA, rB)           ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (215 << 1)) // Store Byte Zero

#define PPC_RLWINM(rA, rS, sh, mb, me) ((21 << 26) | ((rS) << 21) | ((rA) << 16) | ((sh) << 11) | ((mb) << 6) | ((me) << 1))
#define PPC_SRWI(rA, rS, sh)           PPC_RLWINM(rA, rS, (32 - (sh)) & 31, sh, 31)

// -------------------------------------------------------------------------
// ENGINE DEFINITIONS
// -------------------------------------------------------------------------
typedef void (*JITBlockFunc)();

struct JITResult {
    u32 cycles;
    u32 nextPC;
};

struct BasicBlock {
    u32 startPC;
    u32 length;
    JITBlockFunc execute;
};

class BlockCacheManager {
	private:
		static constexpr size_t JIT_ARENA_SIZE = 512 * 1024; // 512 KB
		u32* jitArena;
		size_t arenaOffset;

		static constexpr size_t HASH_TABLE_SIZE = 8192;
		BasicBlock* blockTable[HASH_TABLE_SIZE];

	public:
		BlockCacheManager();
		~BlockCacheManager();

		u32* allocateJITMemory(size_t numBytes);
		void rewindJITMemory(size_t numBytes);
		void registerBlock(BasicBlock* block);
		void flushCache();

		inline BasicBlock* getBlock(u32 pc) const {
			u32 index = ((pc >> 1) ^ (pc >> 13)) & (HASH_TABLE_SIZE - 1);
			BasicBlock* block = blockTable[index];
			if (block && block->startPC == pc) {
				return block;
			}
			return nullptr;
		}
};

extern BlockCacheManager g_blockCache;

extern "C" void ExecuteJITTrace(JITBlockFunc execute, u32* gbaRegs, u32* flags, JITResult* outResult, u8** readPages, u32* readMasks);

BasicBlock* CompileThumbTrace_JIT(u32 startPC, BlockCacheManager& cache);

#endif // BLOCK_CACHE_MANAGER_H

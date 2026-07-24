#ifndef NO_JIT_COMPILER
#ifndef JIT_PPC_EMITTER_H
#define JIT_PPC_EMITTER_H

// -------------------------------------------------------------------------
// POWERPC BROADWAY EMITTER MACROS & REGISTER MAPPINGS
// -------------------------------------------------------------------------
// COMPLETE PPC REGISTER MAP (R0 - R31)
// -------------------------------------------------------------------------
// ABI & SYSTEM RESERVED (DO NOT USE AS GENERAL SCRATCH)
// R0  : Hardware constant '0' in base-addressing and immediate math. NOT a usable scratch register!
// R1  : Host Stack Pointer (used natively by GCC/ABI, e.g., for stack frame offsets like 84(r1)).
// R2  : Host System Table / SDA2 (System ABI reserved, do not touch).
// R13 : Host Small Data Area (SDA) pointer (System ABI reserved, do not touch).
//
// VOLATILE REGISTERS (Host ABI - Clobbered across C++ calls)
// R3  : `execute` arg in / Block epilogue return value (cycles out).
// R4  : `gbaRegs` arg in / Block epilogue return value (next PC out) / instruction-local scratch.
// R5  : `flags` arg in / Live `busPrefetchCount` accumulator.
// R6  : PPC_REG_FLAGS -- all four GBA condition flags (N/Z/C/V) packed into one register
//       (lazily loaded, whole-register dirty-tracked). Bits 0-3 in IBM/rlwinm numbering
//       (i.e. the top nibble, conventional bits 31-28) hold N,Z,C,V respectively; the
//       low 28 bits are unused don't-care space and are never read. See "PACKED FLAGS"
//       below for the insertion/extraction convention shared by the emitter.
// R7  : General scratch
// R8  : General scratch
// R9  : General scratch
// R10 : Scratch: Base Page Pointer / General Math.
// R11 : Scratch: Bank / Mask / Condition / General Math.
// R12 : Scratch: Target Address / Operand / General Math.
//
// NON-VOLATILE REGISTERS (Host ABI - Preserved across C++ calls)
// R14 : PPC_REG_GBA_REGS_PTR (Base pointer to the C++ gbaRegs array).
// R15 : Lazily allocated host pool for GBA R0-R14.
// R16 : Lazily allocated host pool for GBA R0-R14.
// R17 : Lazily allocated host pool for GBA R0-R14.
// R18 : Lazily allocated host pool for GBA R0-R14.
// R19 : Lazily allocated host pool for GBA R0-R14.
// R20 : Lazily allocated host pool for GBA R0-R14.
// R21 : Lazily allocated host pool for GBA R0-R14.
// R22 : Lazily allocated host pool for GBA R0-R14.
// R23 : Lazily allocated host pool for GBA R0-R14.
// R24 : Lazily allocated host pool for GBA R0-R14.
// R25 : Lazily allocated host pool for GBA R0-R14.
// R26 : Lazily allocated host pool for GBA R0-R14.
// R27 : Lazily allocated host pool for GBA R0-R14.
// R28 : Lazily allocated host pool for GBA R0-R14.
// R29 : PPC_REG_PC (GBA R15 / Pipeline PC).
// R30 : PPC_R30_PAGES (readPages Base Pointer Array).
// R31 : PPC_R31_MASKS (readMasks Base Pointer Array).
// -------------------------------------------------------------------------
#define PPC_R3   3
#define PPC_R4   4
#define PPC_R5   5
#define PPC_R6   6   // PPC_REG_FLAGS -- packed N/Z/C/V (see below)
#define PPC_R7   7   // General scratch
#define PPC_R8   8   // General scratch
#define PPC_R9   9   // General scratch
#define PPC_R10  10  // Scratch: Base Page Pointer
#define PPC_R11  11  // Scratch: Bank / Mask / Condition
#define PPC_R12  12  // Scratch: Target Address / Operand

#define PPC_R29  29

#define PPC_R30_PAGES 30  // readPages Base Pointer Array (Non-volatile, no overlap with GBA registers)
#define PPC_R31_MASKS 31  // readMasks Base Pointer Array (Non-volatile, no overlap with GBA registers)

// -------------------------------------------------------------------------
// PACKED FLAGS
// -------------------------------------------------------------------------
// All four GBA condition flags live packed together in one dedicated register,
// PPC_REG_FLAGS (R6), instead of one dedicated register apiece. This frees R7/R8/R9
// back into the general scratch pool at the cost of a couple of extra rlwimi/rlwinm
// ops around each flag access (never around the surrounding ALU math itself).
//
// Layout (rlwinm/rlwimi "IBM" bit numbering, bit 0 = MSB): bits 0,1,2,3 hold N,Z,C,V
// respectively (i.e. conventional bits 31,30,29,28 -- the top nibble). The remaining
// 28 low bits are unused scratch space; nothing ever reads them, so they're left as
// whatever garbage rotate/insert leaves behind.
//
// Every flag-bearing instruction in the JIT already computes each flag as a plain
// 0-or-1 value sitting in a scratch register's bit 31 (LSB) -- that's the natural
// output of the usual rlwinm/cntlzw/mfxer extraction idioms used throughout the
// emitter. The three macros below are the ONLY place that ever needs to know the
// packed bit layout; every call site just names the flag and reuses the exact shift
// amount it would have used for a classic "extract to bit 31" single-bit rlwinm.
#define FLAG_BIT_N 0
#define FLAG_BIT_Z 1
#define FLAG_BIT_C 2
#define FLAG_BIT_V 3

#define PPC_REG_FLAGS PPC_R6

// Merge a flag bit directly into PPC_REG_FLAGS, in one instruction, without disturbing
// the other three packed flags. `srcReg`/`sh` are exactly the register and rotate amount
// you'd pass to a classic single-bit extraction `PPC_RLWINM(dst, srcReg, sh, 31, 31)` --
// this redirects that same extraction to land in `targetBit` of the packed register
// instead of bit 31 of a scratch/dedicated register.
#define PPC_MERGE_FLAG_BIT(targetBit, srcReg, sh) \
	PPC_RLWIMI(PPC_REG_FLAGS, (srcReg), (((sh) + 31 - (targetBit)) & 31), (targetBit), (targetBit))

// Extract flag `targetBit` out of PPC_REG_FLAGS into `dstReg` as a plain 0/1 integer
// (bit 31 / LSB of dstReg), ready for CMPWI-against-zero or arithmetic use (e.g. the
// ADC/SBC carry-in trick).
#define PPC_EXTRACT_FLAG_BIT(dstReg, targetBit) \
	PPC_RLWINM((dstReg), PPC_REG_FLAGS, (((targetBit) + 1) & 31), 31, 31)

// ALU Operations
#define PPC_ADD(rD, rA, rB)    ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (266 << 1))
#define PPC_SUBF(rD, rA, rB)   ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (40 << 1))
#define PPC_AND(rA, rS, rB)    ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (28 << 1))
#define PPC_OR(rA, rS, rB)     ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (444 << 1))
#define PPC_XOR(rA, rS, rB)    ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (316 << 1))
#define PPC_ANDC(rA, rS, rB)   ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (60 << 1))
#define PPC_NOR(rD, rA, rB)    ((31 << 26) | ((rA) << 21) | ((rD) << 16) | ((rB) << 11) | (124 << 1))
#define PPC_MULLW(rD, rA, rB)  ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (235 << 1))
#define PPC_MULLI(rD, rA, imm) ((7 << 26) | ((rD) << 21) | ((rA) << 16) | ((imm) & 0xFFFF))

// Hardware Flag Math (XER & Zero Checks)
#define PPC_ADDCO(rD, rA, rB)	((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (1 << 10) | (10 << 1))
#define PPC_SUBFCO(rD, rA, rB)	((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (1 << 10) | (8 << 1))
#define PPC_ADDEO(rD, rA, rB)   ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (1 << 10) | (138 << 1))
#define PPC_SUBFEO(rD, rA, rB)  ((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (1 << 10) | (136 << 1))
#define PPC_MFXER(rD)			((31 << 26) | ((rD) << 21) | (1 << 16) | (339 << 1))
#define PPC_MTSPR(spr, rS)		((31 << 26) | ((rS) << 21) | ((((spr) & 0x1F) << 16) | (((spr) >> 5) & 0x1F) << 11) | (467 << 1))
#define PPC_MTXER(rt)			PPC_MTSPR(1, (rt))
#define PPC_CNTLZW(rA, rS)		((31 << 26) | ((rS) << 21) | ((rA) << 16) | (26 << 1))

// Immediate Operations
#define PPC_ADDI(rD, rA, val)	((14 << 26) | ((rD) << 21) | ((rA) << 16) | ((val) & 0xFFFF))
#define PPC_LI(rD, val)			PPC_ADDI(rD, 0, val)
#define PPC_LIS(rD, val)		((15 << 26) | ((rD) << 21) | (0 << 16) | ((val) & 0xFFFF))
#define PPC_ORI(rA, rS, val)	((24 << 26) | ((rS) << 21) | ((rA) << 16) | ((val) & 0xFFFF))
#define PPC_ORIS(rA, rS, val)	((25 << 26) | ((rS) << 21) | ((rA) << 16) | ((val) & 0xFFFF))
#define PPC_ADDIC(rD, rA, imm)	((13 << 26) | ((rD) << 21) | ((rA) << 16) | ((imm) & 0xFFFF))

// Branches
#define PPC_CMPWI(cr, rA, imm)	((11 << 26) | ((cr) << 23) | ((rA) << 16) | ((imm) & 0xFFFF))
#define PPC_BNE(offset)			((16 << 26) | (4 << 21) | (2 << 16) | ((offset) & 0xFFFC))
#define PPC_BEQ(offset)			((16 << 26) | (12 << 21) | (2 << 16) | ((offset) & 0xFFFC))
#define PPC_BGE(offset)			((16 << 26) | (4 << 21) | (0 << 16) | ((offset) & 0xFFFC))
#define PPC_B(offset)			((18 << 26) | ((offset) & 0x3FFFFFC))
#define PPC_BLE(offset)			((16 << 26) | (4 << 21) | (1 << 16) | ((offset) & 0xFFFC))
#define PPC_BLT(offset)			((16 << 26) | (12 << 21) | (0 << 16) | ((offset) & 0xFFFC))
#define PPC_BLR()				0x4E800020

// Advanced Memory & Bitwise
// Endian-Correct Memory Loads & Stores (GBA is Little-Endian, Wii is Big-Endian)
#define PPC_LWZX(rD, rA, rB)			((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (23 << 1))
#define PPC_LWBRX(rD, rA, rB)			((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (534 << 1)) // Load Word Byte-Reverse
#define PPC_LHBRX(rD, rA, rB)			((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (790 << 1)) // Load Halfword Byte-Reverse
#define PPC_LBZX(rD, rA, rB)			((31 << 26) | ((rD) << 21) | ((rA) << 16) | ((rB) << 11) | (87 << 1))  // Load Byte Zero

#define PPC_STWBRX(rS, rA, rB)          ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (662 << 1)) // Store Word Byte-Reverse
#define PPC_STHBRX(rS, rA, rB)          ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (918 << 1)) // Store Halfword Byte-Reverse
#define PPC_STBZX(rS, rA, rB)           ((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (215 << 1)) // Store Byte Zero

#define PPC_RLWINM(rA, rS, sh, mb, me)	((21 << 26) | ((rS) << 21) | ((rA) << 16) | ((sh) << 11) | ((mb) << 6) | ((me) << 1))
#define PPC_RLWNM(rA, rS, rB, mb, me)	((23 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | ((mb) << 6) | ((me) << 1))
#define PPC_RLWIMI(rA, rS, sh, mb, me)	((20 << 26) | ((rS) << 21) | ((rA) << 16) | ((sh) << 11) | ((mb) << 6) | ((me) << 1))
#define PPC_SRWI(rA, rS, sh)			PPC_RLWINM(rA, rS, (32 - (sh)) & 31, sh, 31)
#define PPC_SRAW(rA, rS, rB)			((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (792 << 1))
#define PPC_SRAWI(rA, rS, sh)			((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((sh) << 11) | (824 << 1))
#define PPC_SRW(rA, rS, rB)    			((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (536 << 1))
#define PPC_SLW(rA, rS, rB)    			((31 << 26) | ((rS) << 21) | ((rA) << 16) | ((rB) << 11) | (24 << 1))

#define PPC_EXTSB(rA, rS) ((31 << 26) | ((rS) << 21) | ((rA) << 16) | (954 << 1)) // Extend Sign Byte
#define PPC_EXTSH(rA, rS) ((31 << 26) | ((rS) << 21) | ((rA) << 16) | (922 << 1)) // Extend Sign Halfword

// -------------------------------------------------------------------------
// LINKER & PATCHING MACROS
// -------------------------------------------------------------------------
#define PPC_MFLR(rD)           ((31 << 26) | ((rD) << 21) | (256 << 11) | (339 << 1))
#define PPC_LWZ(rD, rA, d)     ((32 << 26) | ((rD) << 21) | ((rA) << 16) | ((d) & 0xFFFF))
#define PPC_STW(rS, rA, d)     ((36 << 26) | ((rS) << 21) | ((rA) << 16) | ((d) & 0xFFFF))
#define PPC_CMPW(cr, rA, rB)   ((31 << 26) | ((cr) << 23) | ((rA) << 16) | ((rB) << 11) | (0 << 1))

// Hardware Cache Maintenance
#define PPC_DCBST(rA, rB)      ((31 << 26) | ((rA) << 16) | ((rB) << 11) | (54 << 1))
#define PPC_ICBI(rA, rB)       ((31 << 26) | ((rA) << 16) | ((rB) << 11) | (982 << 1))
#define PPC_SYNC()             ((31 << 26) | (598 << 1))
#define PPC_ISYNC()            ((19 << 26) | (150 << 1))

// Dynamic Register Branching
#define PPC_MTCTR(rS)          ((31 << 26) | ((rS) << 21) | (288 << 11) | (467 << 1))
#define PPC_BCTR()             ((19 << 26) | (20 << 21) | (528 << 1))
#define PPC_BL(offset)         ((18 << 26) | ((offset) & 0x3FFFFFC) | 1)

#endif // JIT_PPC_EMITTER_H
#endif

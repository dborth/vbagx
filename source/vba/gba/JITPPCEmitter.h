#ifndef NO_JIT_COMPILER
#ifndef JIT_PPC_EMITTER_H
#define JIT_PPC_EMITTER_H

// -------------------------------------------------------------------------
// POWERPC BROADWAY EMITTER MACROS & REGISTER MAPPINGS
// -------------------------------------------------------------------------
#define PPC_R0   0
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

#define PPC_R29  29

#define PPC_R30_PAGES 30  // readPages Base Pointer Array (Non-volatile, no overlap with GBA registers)
#define PPC_R31_MASKS 31  // readMasks Base Pointer Array (Non-volatile, no overlap with GBA registers)

#define PPC_REG_C PPC_R6
#define PPC_REG_V PPC_R7
#define PPC_REG_N PPC_R8
#define PPC_REG_Z PPC_R9

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
#define PPC_CNTLZW(rA, rS)     ((31 << 26) | ((rS) << 21) | ((rA) << 16) | (26 << 1))

// Immediate Operations
#define PPC_ADDI(rD, rA, val)  ((14 << 26) | ((rD) << 21) | ((rA) << 16) | ((val) & 0xFFFF))
#define PPC_LI(rD, val)        PPC_ADDI(rD, 0, val)
#define PPC_LIS(rD, val)       ((15 << 26) | ((rD) << 21) | (0 << 16) | ((val) & 0xFFFF))
#define PPC_ORI(rA, rS, val)   ((24 << 26) | ((rS) << 21) | ((rA) << 16) | ((val) & 0xFFFF))
#define PPC_ADDIC(rD, rA, imm)  ((13 << 26) | ((rD) << 21) | ((rA) << 16) | ((imm) & 0xFFFF))

// Branches
#define PPC_CMPWI(cr, rA, imm) ((11 << 26) | ((cr) << 23) | ((rA) << 16) | ((imm) & 0xFFFF))
#define PPC_BNE(offset)        ((16 << 26) | (4 << 21) | (2 << 16) | ((offset) & 0xFFFC))
#define PPC_BEQ(offset)        ((16 << 26) | (12 << 21) | (2 << 16) | ((offset) & 0xFFFC))
#define PPC_BGE(offset)        ((16 << 26) | (4 << 21) | (0 << 16) | ((offset) & 0xFFFC))
#define PPC_B(offset)          ((18 << 26) | ((offset) & 0x3FFFFFC))
#define PPC_BLE(offset)        ((16 << 26) | (4 << 21) | (1 << 16) | ((offset) & 0xFFFC))
#define PPC_BLT(offset)        ((16 << 26) | (12 << 21) | (0 << 16) | ((offset) & 0xFFFC))
#define PPC_BLR()              0x4E800020

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
#define PPC_SRWI(rA, rS, sh)			PPC_RLWINM(rA, rS, (32 - (sh)) & 31, sh, 31)
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef JIT_COMPILER_DIFFERENTIAL_TESTING
#include <string>
#endif

#include "GBA.h"
#include "GBAcpu.h"
#include "GBAinline.h"
#include "JITCache.h"
#include "JITProfiler.h"
#include "Globals.h"
#include "EEprom.h"
#include "Flash.h"
#include "Sound.h"
#include "Sram.h"
#include "bios.h"
#include "Cheats.h"
#include "../NLS.h"
#include "../Util.h"
#include "../System.h"

///////////////////////////////////////////////////////////////////////////

static int clockTicks;

static INSN_REGPARM void thumbUnknownInsn(u32 opcode)
{
  CPUUndefinedException();
}

// Common macros //////////////////////////////////////////////////////////

// Broadway Optimization: Branchless Memory Prefetch Evaluation
// Replaces volatile boolean comparisons with pure 1-cycle bitwise logic
// Logic: ~((v | -v) >> 31) & 1 yields 1 ONLY if v is exactly 0.
#define UPDATE_BUS_PREFETCH \
    busPrefetch |= (busPrefetchEnable & (~((busPrefetchCount | -busPrefetchCount) >> 31) & 1));

#define NEG(i) ((i) >> 31)
#define POS(i) ((~(i)) >> 31)

#define ADDCARRY(a, b, c) \
  C_FLAG = (((a) & (b)) | ((a) & ~(c)) | ((b) & ~(c))) >> 31;

#define ADDOVERFLOW(a, b, c) \
  V_FLAG = (((a) & (b) & ~(c)) | (~(a) & ~(b) & (c))) >> 31;

#define SUBCARRY(a, b, c) \
  C_FLAG = (((a) & ~(b)) | ((a) & ~(c)) | (~(b) & ~(c))) >> 31;

#define SUBOVERFLOW(a, b, c)\
  V_FLAG = (((a) & ~(b) & ~(c)) | (~(a) & (b) & (c))) >> 31;

#define ADD_RD_RS_RN(N) \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = reg[N].I;\
     u32 res = lhs + rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#ifndef ADD_RD_RS_O3
#define ADD_RD_RS_O3(N) \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = N;\
     u32 res = lhs + rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#endif
# define ADD_RD_RS_O3_0 ADD_RD_RS_O3
#define ADD_RN_O8(d) \
   {\
     u32 lhs = reg[(d)].I;\
     u32 rhs = (opcode & 255);\
     u32 res = lhs + rhs;\
     reg[(d)].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define CMN_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     u32 res = lhs + rhs;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define ADC_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     u32 res = lhs + rhs + (u32)C_FLAG;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     ADDCARRY(lhs, rhs, res);\
     ADDOVERFLOW(lhs, rhs, res);\
   }
#define SUB_RD_RS_RN(N) \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = reg[N].I;\
     u32 res = lhs - rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define SUB_RD_RS_O3(N) \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = N;\
     u32 res = lhs - rhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
# define SUB_RD_RS_O3_0 SUB_RD_RS_O3
#define SUB_RN_O8(d) \
   {\
     u32 lhs = reg[(d)].I;\
     u32 rhs = (opcode & 255);\
     u32 res = lhs - rhs;\
     reg[(d)].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define MOV_RN_O8(d) \
   {\
     reg[(d)].I = opcode & 255;\
     N_FLAG = 0;\
     Z_FLAG = (reg[(d)].I == 0);\
   }
#define CMP_RN_O8(d) \
   {\
     u32 lhs = reg[(d)].I;\
     u32 rhs = (opcode & 255);\
     u32 res = lhs - rhs;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define SBC_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     /* Branchless NOT for C_FLAG since it is strictly 0 or 1 */ \
     u32 res = lhs - rhs - (C_FLAG ^ 1);\
     reg[dest].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define LSL_RD_RM_I5 \
   {\
     C_FLAG = (reg[source].I >> (32 - shift)) & 1;\
     value = reg[source].I << shift;\
   }
#define LSL_RD_RS \
   {\
     C_FLAG = (reg[dest].I >> (32 - value)) & 1;\
     value = reg[dest].I << value;\
   }
#define LSR_RD_RM_I5 \
   {\
     C_FLAG = (reg[source].I >> (shift - 1)) & 1;\
     value = reg[source].I >> shift;\
   }
#define LSR_RD_RS \
   {\
     C_FLAG = (reg[dest].I >> (value - 1)) & 1;\
     value = reg[dest].I >> value;\
   }
#define ASR_RD_RM_I5 \
   {\
     C_FLAG = ((u32)((s32)reg[source].I >> (int)(shift - 1))) & 1;\
     value = (u32)((s32)reg[source].I >> (int)shift);\
   }
#define ASR_RD_RS \
   {\
     C_FLAG = ((u32)((s32)reg[dest].I >> (int)(value - 1))) & 1;\
     value = (u32)((s32)reg[dest].I >> (int)value);\
   }
#define ROR_RD_RS \
   {\
     C_FLAG = (reg[dest].I >> (value - 1)) & 1;\
     value = ((reg[dest].I << (32 - value)) |\
              (reg[dest].I >> value));\
   }
#define NEG_RD_RS \
   {\
     u32 lhs = reg[source].I;\
     u32 rhs = 0;\
     u32 res = rhs - lhs;\
     reg[dest].I = res;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     SUBCARRY(rhs, lhs, res);\
     SUBOVERFLOW(rhs, lhs, res);\
   }
#define CMP_RD_RS \
   {\
     u32 lhs = reg[dest].I;\
     u32 rhs = value;\
     u32 res = lhs - rhs;\
     Z_FLAG = (res == 0);\
     N_FLAG = (res >> 31);\
     SUBCARRY(lhs, rhs, res);\
     SUBOVERFLOW(lhs, rhs, res);\
   }
#define IMM5_INSN(OP,N) \
  int dest = opcode & 0x07;\
  int source = (opcode >> 3) & 0x07;\
  u32 value;\
  OP(N);\
  reg[dest].I = value;\
  N_FLAG = (value >> 31);\
  Z_FLAG = (value == 0);
#define IMM5_INSN_0(OP) \
  int dest = opcode & 0x07;\
  int source = (opcode >> 3) & 0x07;\
  u32 value;\
  OP;\
  reg[dest].I = value;\
  N_FLAG = (value >> 31);\
  Z_FLAG = (value == 0);
#define IMM5_LSL(N) \
  int shift = N;\
  LSL_RD_RM_I5;
#define IMM5_LSL_0 \
  value = reg[source].I;
#define IMM5_LSR(N) \
  int shift = N;\
  LSR_RD_RM_I5;
#define IMM5_LSR_0 \
  C_FLAG = (reg[source].I >> 31);\
  value = 0;
#define IMM5_ASR(N) \
  int shift = N;\
  ASR_RD_RM_I5;
#define IMM5_ASR_0 \
  C_FLAG = (reg[source].I >> 31);\
  value = (u32)((s32)reg[source].I >> 31);
#ifndef THREEARG_INSN
 #define THREEARG_INSN(OP,N) \
  int dest = opcode & 0x07;          \
  int source = (opcode >> 3) & 0x07; \
  OP(N);
#endif

// Shift instructions /////////////////////////////////////////////////////

#define DEFINE_IMM5_INSN(OP,BASE) \
  static INSN_REGPARM void thumb##BASE##_00(u32 opcode) { IMM5_INSN_0(OP##_0); } \
  static INSN_REGPARM void thumb##BASE##_01(u32 opcode) { IMM5_INSN(OP, 1); } \
  static INSN_REGPARM void thumb##BASE##_02(u32 opcode) { IMM5_INSN(OP, 2); } \
  static INSN_REGPARM void thumb##BASE##_03(u32 opcode) { IMM5_INSN(OP, 3); } \
  static INSN_REGPARM void thumb##BASE##_04(u32 opcode) { IMM5_INSN(OP, 4); } \
  static INSN_REGPARM void thumb##BASE##_05(u32 opcode) { IMM5_INSN(OP, 5); } \
  static INSN_REGPARM void thumb##BASE##_06(u32 opcode) { IMM5_INSN(OP, 6); } \
  static INSN_REGPARM void thumb##BASE##_07(u32 opcode) { IMM5_INSN(OP, 7); } \
  static INSN_REGPARM void thumb##BASE##_08(u32 opcode) { IMM5_INSN(OP, 8); } \
  static INSN_REGPARM void thumb##BASE##_09(u32 opcode) { IMM5_INSN(OP, 9); } \
  static INSN_REGPARM void thumb##BASE##_0A(u32 opcode) { IMM5_INSN(OP,10); } \
  static INSN_REGPARM void thumb##BASE##_0B(u32 opcode) { IMM5_INSN(OP,11); } \
  static INSN_REGPARM void thumb##BASE##_0C(u32 opcode) { IMM5_INSN(OP,12); } \
  static INSN_REGPARM void thumb##BASE##_0D(u32 opcode) { IMM5_INSN(OP,13); } \
  static INSN_REGPARM void thumb##BASE##_0E(u32 opcode) { IMM5_INSN(OP,14); } \
  static INSN_REGPARM void thumb##BASE##_0F(u32 opcode) { IMM5_INSN(OP,15); } \
  static INSN_REGPARM void thumb##BASE##_10(u32 opcode) { IMM5_INSN(OP,16); } \
  static INSN_REGPARM void thumb##BASE##_11(u32 opcode) { IMM5_INSN(OP,17); } \
  static INSN_REGPARM void thumb##BASE##_12(u32 opcode) { IMM5_INSN(OP,18); } \
  static INSN_REGPARM void thumb##BASE##_13(u32 opcode) { IMM5_INSN(OP,19); } \
  static INSN_REGPARM void thumb##BASE##_14(u32 opcode) { IMM5_INSN(OP,20); } \
  static INSN_REGPARM void thumb##BASE##_15(u32 opcode) { IMM5_INSN(OP,21); } \
  static INSN_REGPARM void thumb##BASE##_16(u32 opcode) { IMM5_INSN(OP,22); } \
  static INSN_REGPARM void thumb##BASE##_17(u32 opcode) { IMM5_INSN(OP,23); } \
  static INSN_REGPARM void thumb##BASE##_18(u32 opcode) { IMM5_INSN(OP,24); } \
  static INSN_REGPARM void thumb##BASE##_19(u32 opcode) { IMM5_INSN(OP,25); } \
  static INSN_REGPARM void thumb##BASE##_1A(u32 opcode) { IMM5_INSN(OP,26); } \
  static INSN_REGPARM void thumb##BASE##_1B(u32 opcode) { IMM5_INSN(OP,27); } \
  static INSN_REGPARM void thumb##BASE##_1C(u32 opcode) { IMM5_INSN(OP,28); } \
  static INSN_REGPARM void thumb##BASE##_1D(u32 opcode) { IMM5_INSN(OP,29); } \
  static INSN_REGPARM void thumb##BASE##_1E(u32 opcode) { IMM5_INSN(OP,30); } \
  static INSN_REGPARM void thumb##BASE##_1F(u32 opcode) { IMM5_INSN(OP,31); }

// LSL Rd, Rm, #Imm 5
DEFINE_IMM5_INSN(IMM5_LSL,00)
// LSR Rd, Rm, #Imm 5
DEFINE_IMM5_INSN(IMM5_LSR,08)
// ASR Rd, Rm, #Imm 5
DEFINE_IMM5_INSN(IMM5_ASR,10)

// 3-argument ADD/SUB /////////////////////////////////////////////////////

#define DEFINE_REG3_INSN(OP,BASE) \
  static INSN_REGPARM void thumb##BASE##_0(u32 opcode) { THREEARG_INSN(OP,0); } \
  static INSN_REGPARM void thumb##BASE##_1(u32 opcode) { THREEARG_INSN(OP,1); } \
  static INSN_REGPARM void thumb##BASE##_2(u32 opcode) { THREEARG_INSN(OP,2); } \
  static INSN_REGPARM void thumb##BASE##_3(u32 opcode) { THREEARG_INSN(OP,3); } \
  static INSN_REGPARM void thumb##BASE##_4(u32 opcode) { THREEARG_INSN(OP,4); } \
  static INSN_REGPARM void thumb##BASE##_5(u32 opcode) { THREEARG_INSN(OP,5); } \
  static INSN_REGPARM void thumb##BASE##_6(u32 opcode) { THREEARG_INSN(OP,6); } \
  static INSN_REGPARM void thumb##BASE##_7(u32 opcode) { THREEARG_INSN(OP,7); }

#define DEFINE_IMM3_INSN(OP,BASE) \
  static INSN_REGPARM void thumb##BASE##_0(u32 opcode) { THREEARG_INSN(OP##_0,0); } \
  static INSN_REGPARM void thumb##BASE##_1(u32 opcode) { THREEARG_INSN(OP,1); } \
  static INSN_REGPARM void thumb##BASE##_2(u32 opcode) { THREEARG_INSN(OP,2); } \
  static INSN_REGPARM void thumb##BASE##_3(u32 opcode) { THREEARG_INSN(OP,3); } \
  static INSN_REGPARM void thumb##BASE##_4(u32 opcode) { THREEARG_INSN(OP,4); } \
  static INSN_REGPARM void thumb##BASE##_5(u32 opcode) { THREEARG_INSN(OP,5); } \
  static INSN_REGPARM void thumb##BASE##_6(u32 opcode) { THREEARG_INSN(OP,6); } \
  static INSN_REGPARM void thumb##BASE##_7(u32 opcode) { THREEARG_INSN(OP,7); }

// ADD Rd, Rs, Rn
DEFINE_REG3_INSN(ADD_RD_RS_RN,18)
// SUB Rd, Rs, Rn
DEFINE_REG3_INSN(SUB_RD_RS_RN,1A)
// ADD Rd, Rs, #Offset3
DEFINE_IMM3_INSN(ADD_RD_RS_O3,1C)
// SUB Rd, Rs, #Offset3
DEFINE_IMM3_INSN(SUB_RD_RS_O3,1E)

// MOV/CMP/ADD/SUB immediate //////////////////////////////////////////////

// MOV R0, #Offset8
static INSN_REGPARM void thumb20(u32 opcode) { MOV_RN_O8(0); }
// MOV R1, #Offset8
static INSN_REGPARM void thumb21(u32 opcode) { MOV_RN_O8(1); }
// MOV R2, #Offset8
static INSN_REGPARM void thumb22(u32 opcode) { MOV_RN_O8(2); }
// MOV R3, #Offset8
static INSN_REGPARM void thumb23(u32 opcode) { MOV_RN_O8(3); }
// MOV R4, #Offset8
static INSN_REGPARM void thumb24(u32 opcode) { MOV_RN_O8(4); }
// MOV R5, #Offset8
static INSN_REGPARM void thumb25(u32 opcode) { MOV_RN_O8(5); }
// MOV R6, #Offset8
static INSN_REGPARM void thumb26(u32 opcode) { MOV_RN_O8(6); }
// MOV R7, #Offset8
static INSN_REGPARM void thumb27(u32 opcode) { MOV_RN_O8(7); }

// CMP R0, #Offset8
static INSN_REGPARM void thumb28(u32 opcode) { CMP_RN_O8(0); }
// CMP R1, #Offset8
static INSN_REGPARM void thumb29(u32 opcode) { CMP_RN_O8(1); }
// CMP R2, #Offset8
static INSN_REGPARM void thumb2A(u32 opcode) { CMP_RN_O8(2); }
// CMP R3, #Offset8
static INSN_REGPARM void thumb2B(u32 opcode) { CMP_RN_O8(3); }
// CMP R4, #Offset8
static INSN_REGPARM void thumb2C(u32 opcode) { CMP_RN_O8(4); }
// CMP R5, #Offset8
static INSN_REGPARM void thumb2D(u32 opcode) { CMP_RN_O8(5); }
// CMP R6, #Offset8
static INSN_REGPARM void thumb2E(u32 opcode) { CMP_RN_O8(6); }
// CMP R7, #Offset8
static INSN_REGPARM void thumb2F(u32 opcode) { CMP_RN_O8(7); }

// ADD R0,#Offset8
static INSN_REGPARM void thumb30(u32 opcode) { ADD_RN_O8(0); }
// ADD R1,#Offset8
static INSN_REGPARM void thumb31(u32 opcode) { ADD_RN_O8(1); }
// ADD R2,#Offset8
static INSN_REGPARM void thumb32(u32 opcode) { ADD_RN_O8(2); }
// ADD R3,#Offset8
static INSN_REGPARM void thumb33(u32 opcode) { ADD_RN_O8(3); }
// ADD R4,#Offset8
static INSN_REGPARM void thumb34(u32 opcode) { ADD_RN_O8(4); }
// ADD R5,#Offset8
static INSN_REGPARM void thumb35(u32 opcode) { ADD_RN_O8(5); }
// ADD R6,#Offset8
static INSN_REGPARM void thumb36(u32 opcode) { ADD_RN_O8(6); }
// ADD R7,#Offset8
static INSN_REGPARM void thumb37(u32 opcode) { ADD_RN_O8(7); }

// SUB R0,#Offset8
static INSN_REGPARM void thumb38(u32 opcode) { SUB_RN_O8(0); }
// SUB R1,#Offset8
static INSN_REGPARM void thumb39(u32 opcode) { SUB_RN_O8(1); }
// SUB R2,#Offset8
static INSN_REGPARM void thumb3A(u32 opcode) { SUB_RN_O8(2); }
// SUB R3,#Offset8
static INSN_REGPARM void thumb3B(u32 opcode) { SUB_RN_O8(3); }
// SUB R4,#Offset8
static INSN_REGPARM void thumb3C(u32 opcode) { SUB_RN_O8(4); }
// SUB R5,#Offset8
static INSN_REGPARM void thumb3D(u32 opcode) { SUB_RN_O8(5); }
// SUB R6,#Offset8
static INSN_REGPARM void thumb3E(u32 opcode) { SUB_RN_O8(6); }
// SUB R7,#Offset8
static INSN_REGPARM void thumb3F(u32 opcode) { SUB_RN_O8(7); }

// ALU operations /////////////////////////////////////////////////////////

// AND Rd, Rs
static INSN_REGPARM void thumb40_0(u32 opcode)
{
  int dest = opcode & 7;
  reg[dest].I &= reg[(opcode >> 3)&7].I;
  N_FLAG = (reg[dest].I >> 31);
  Z_FLAG = (reg[dest].I == 0);
}

// EOR Rd, Rs
static INSN_REGPARM void thumb40_1(u32 opcode)
{
  int dest = opcode & 7;
  reg[dest].I ^= reg[(opcode >> 3)&7].I;
  N_FLAG = (reg[dest].I >> 31);
  Z_FLAG = (reg[dest].I == 0);
}

// LSL Rd, Rs
static INSN_REGPARM void thumb40_2(u32 opcode)
{
  int dest = opcode & 7;
  u32 value = reg[(opcode >> 3)&7].B.B0;

  if(value) {
    u32 is32 = (value == 32);
    u32 isUnder32 = (value < 32);

    // Branchless flag and shift evaluation using Broadway's dual IUs
    C_FLAG = (is32 & (reg[dest].I & 1)) | (isUnder32 & ((reg[dest].I >> (32 - (value & 31))) & 1));
    value = isUnder32 * (reg[dest].I << (value & 31));

    reg[dest].I = value;
  }
  N_FLAG = (reg[dest].I >> 31);
  Z_FLAG = (reg[dest].I == 0);
  clockTicks = codeTicksAccess16(armNextPC)+2;
}

// LSR Rd, Rs
static INSN_REGPARM void thumb40_3(u32 opcode)
{
    int dest = opcode & 7;
    u32 shift = reg[(opcode >> 3) & 7].B.B0;
    u32 val = reg[dest].I;

    // Generate masks
    u32 mask_not_0 = -(u32)(shift != 0);
    u32 mask_less_32 = -(u32)(shift < 32);

    // Shift result logic (shift >= 32 yields 0)
    u32 res_shifted = (val >> (shift & 0x1F)) & mask_less_32;
    reg[dest].I = (res_shifted & mask_not_0) | (val & ~mask_not_0);

    // C_FLAG logic (shift == 32 yields bit 31; shift > 32 yields 0)
    u32 c_shift = shift - 1;
    u32 mask_c_valid = -(u32)(c_shift < 32);
    u32 new_c = ((val >> (c_shift & 0x1F)) & 1) & mask_c_valid;

    C_FLAG = (new_c & mask_not_0) | (C_FLAG & ~mask_not_0);

    N_FLAG = (reg[dest].I >> 31);
    Z_FLAG = (reg[dest].I == 0);
    clockTicks = codeTicksAccess16(armNextPC) + 2;
}

// ASR Rd, Rs
static INSN_REGPARM void thumb41_0(u32 opcode)
{
    int dest = opcode & 7;
    u32 shift = reg[(opcode >> 3) & 7].B.B0;
    s32 val = (s32)reg[dest].I;

    u32 mask_not_0 = -(u32)(shift != 0);
    u32 mask_less_32 = -(u32)(shift < 32);

    // If shift >= 32, clamp to 31 to propagate the sign bit across the whole register
    u32 shift_clamped = (shift & mask_less_32) | (31 & ~mask_less_32);
    u32 res_shifted = (u32)(val >> shift_clamped);

    reg[dest].I = (res_shifted & mask_not_0) | ((u32)val & ~mask_not_0);

    // C_FLAG logic: bit 31 if shift >= 32
    u32 c_shift = shift - 1;
    u32 c_shift_clamped = (c_shift & mask_less_32) | (31 & ~mask_less_32);
    u32 new_c = ((u32)val >> c_shift_clamped) & 1;

    C_FLAG = (new_c & mask_not_0) | (C_FLAG & ~mask_not_0);

    N_FLAG = (reg[dest].I >> 31);
    Z_FLAG = (reg[dest].I == 0);
    clockTicks = codeTicksAccess16(armNextPC) + 2;
}

// ADC Rd, Rs
static INSN_REGPARM void thumb41_1(u32 opcode)
{
  int dest = opcode & 0x07;
  u32 value = reg[(opcode >> 3)&7].I;
  ADC_RD_RS;
}

// SBC Rd, Rs
static INSN_REGPARM void thumb41_2(u32 opcode)
{
  int dest = opcode & 0x07;
  u32 value = reg[(opcode >> 3)&7].I;
  SBC_RD_RS;
}

// ROR Rd, Rs
static INSN_REGPARM void thumb41_3(u32 opcode)
{
    int dest = opcode & 7;
    u32 shift = reg[(opcode >> 3) & 7].B.B0;
    u32 val = reg[dest].I;

    u32 mask_not_0 = -(u32)(shift != 0);

    // Safe branchless ROR (avoids C++ Undefined Behavior when s == 0)
    // ((-s) & 0x1F) safely wraps back to 0 without triggering a << 32
    u32 s = shift & 0x1F;
    u32 res_shifted = (val >> s) | (val << ((-s) & 0x1F));

    reg[dest].I = (res_shifted & mask_not_0) | (val & ~mask_not_0);

    // C_FLAG logic: for ROR, C_FLAG is always bit 31 of the result if shift != 0
    u32 new_c = (res_shifted >> 31) & 1;
    C_FLAG = (new_c & mask_not_0) | (C_FLAG & ~mask_not_0);

    N_FLAG = (reg[dest].I >> 31);
    Z_FLAG = (reg[dest].I == 0);
    clockTicks = codeTicksAccess16(armNextPC) + 2;
}

// TST Rd, Rs
static INSN_REGPARM void thumb42_0(u32 opcode)
{
  u32 value = reg[opcode & 7].I & reg[(opcode >> 3) & 7].I;
  N_FLAG = (value >> 31);
  Z_FLAG = (value == 0);
}

// NEG Rd, Rs
static INSN_REGPARM void thumb42_1(u32 opcode)
{
  int dest = opcode & 7;
  int source = (opcode >> 3) & 7;
  NEG_RD_RS;
}

// CMP Rd, Rs
static INSN_REGPARM void thumb42_2(u32 opcode)
{
  int dest = opcode & 7;
  u32 value = reg[(opcode >> 3)&7].I;
  CMP_RD_RS;
}

// CMN Rd, Rs
static INSN_REGPARM void thumb42_3(u32 opcode)
{
  int dest = opcode & 7;
  u32 value = reg[(opcode >> 3)&7].I;
  CMN_RD_RS;
}

// ORR Rd, Rs
static INSN_REGPARM void thumb43_0(u32 opcode)
{
  int dest = opcode & 7;
  reg[dest].I |= reg[(opcode >> 3) & 7].I;
  Z_FLAG = (reg[dest].I == 0);
  N_FLAG = (reg[dest].I >> 31);
}

// MUL Rd, Rs
static INSN_REGPARM void thumb43_1(u32 opcode)
{
  clockTicks = 1;
  int dest = opcode & 7;
  u32 rm = reg[dest].I;

  // Calculate result
  reg[dest].I = reg[(opcode >> 3) & 7].I * rm;

  // Convert negative rm for bit-length evaluation
  if (((s32)rm) < 0) {
    rm = ~rm;
  }

  // Broadway Optimization: Branchless magnitude check via hardware cntlzw
  // "rm | 1" ensures we don't pass 0 to clz (which is undefined behavior).
  u32 active_bits = 31 - __builtin_clz(rm | 1);
  clockTicks += (active_bits >> 3); // Maps 0-8 to 0, 9-16 to 1, 17-24 to 2, 25-32 to 3.

  busPrefetchCount = (busPrefetchCount<<clockTicks) | (0xFF>>(8-clockTicks));
  clockTicks += codeTicksAccess16(armNextPC) + 1;
  Z_FLAG = (reg[dest].I == 0);
  N_FLAG = (reg[dest].I >> 31);
}

// BIC Rd, Rs
static INSN_REGPARM void thumb43_2(u32 opcode)
{
  int dest = opcode & 7;
  reg[dest].I &= (~reg[(opcode >> 3) & 7].I);
  Z_FLAG = (reg[dest].I == 0);
  N_FLAG = (reg[dest].I >> 31);
}

// MVN Rd, Rs
static INSN_REGPARM void thumb43_3(u32 opcode)
{
  int dest = opcode & 7;
  reg[dest].I = ~reg[(opcode >> 3) & 7].I;
  Z_FLAG = (reg[dest].I == 0);
  N_FLAG = (reg[dest].I >> 31);
}

// High-register instructions and BX //////////////////////////////////////

// ADD Rd, Hs
static INSN_REGPARM void thumb44_1(u32 opcode)
{
  reg[opcode&7].I += reg[((opcode>>3)&7)+8].I;
}

// ADD Hd, Rs
static INSN_REGPARM void thumb44_2(u32 opcode)
{
  reg[(opcode&7)+8].I += reg[(opcode>>3)&7].I;
  if((opcode&7) == 7) {
    reg[15].I &= 0xFFFFFFFE;
    armNextPC = reg[15].I;
    reg[15].I += 2;
    THUMB_PREFETCH;
    clockTicks = codeTicksAccessSeq16(armNextPC)*2
        + codeTicksAccess16(armNextPC) + 3;
  }
}

// ADD Hd, Hs
static INSN_REGPARM void thumb44_3(u32 opcode)
{
  reg[(opcode&7)+8].I += reg[((opcode>>3)&7)+8].I;
  if((opcode&7) == 7) {
    reg[15].I &= 0xFFFFFFFE;
    armNextPC = reg[15].I;
    reg[15].I += 2;
    THUMB_PREFETCH;
    clockTicks = codeTicksAccessSeq16(armNextPC)*2
        + codeTicksAccess16(armNextPC) + 3;
  }
}

// CMP Rd, Hs
static INSN_REGPARM void thumb45_1(u32 opcode)
{
  int dest = opcode & 7;
  u32 value = reg[((opcode>>3)&7)+8].I;
  CMP_RD_RS;
}

// CMP Hd, Rs
static INSN_REGPARM void thumb45_2(u32 opcode)
{
  int dest = (opcode & 7) + 8;
  u32 value = reg[(opcode>>3)&7].I;
  CMP_RD_RS;
}

// CMP Hd, Hs
static INSN_REGPARM void thumb45_3(u32 opcode)
{
  int dest = (opcode & 7) + 8;
  u32 value = reg[((opcode>>3)&7)+8].I;
  CMP_RD_RS;
}

// MOV Rd, Rs
static INSN_REGPARM void thumb46_0(u32 opcode)
{
	reg[opcode&7].I = reg[((opcode>>3)&7)].I;
	clockTicks = codeTicksAccessSeq16(armNextPC) + 1;
}

// MOV Rd, Hs
static INSN_REGPARM void thumb46_1(u32 opcode)
{
  reg[opcode&7].I = reg[((opcode>>3)&7)+8].I;
  clockTicks = codeTicksAccessSeq16(armNextPC) + 1;
}

// MOV Hd, Rs
static INSN_REGPARM void thumb46_2(u32 opcode)
{
  reg[(opcode&7)+8].I = reg[(opcode>>3)&7].I;
  if((opcode&7) == 7) {
    reg[15].I &= 0xFFFFFFFE;
    armNextPC = reg[15].I;
    reg[15].I += 2;
    THUMB_PREFETCH;
    clockTicks = codeTicksAccessSeq16(armNextPC)*2
        + codeTicksAccess16(armNextPC) + 3;
  }
}

// MOV Hd, Hs
static INSN_REGPARM void thumb46_3(u32 opcode)
{
  reg[(opcode&7)+8].I = reg[((opcode>>3)&7)+8].I;
  if((opcode&7) == 7) {
    reg[15].I &= 0xFFFFFFFE;
    armNextPC = reg[15].I;
    reg[15].I += 2;
    THUMB_PREFETCH;
    clockTicks = codeTicksAccessSeq16(armNextPC)*2
        + codeTicksAccess16(armNextPC) + 3;
  }
}


// BX Rs
static INSN_REGPARM void thumb47(u32 opcode)
{
  int base = (opcode >> 3) & 15;
  busPrefetchCount=0;
  reg[15].I = reg[base].I;
  if(reg[base].I & 1) {
    armState = false;
    reg[15].I &= 0xFFFFFFFE;
    armNextPC = reg[15].I;
    reg[15].I += 2;
    THUMB_PREFETCH;
    clockTicks = codeTicksAccessSeq16(armNextPC)*2 + codeTicksAccess16(armNextPC) + 3;
  } else {
    armState = true;
    reg[15].I &= 0xFFFFFFFC;
    armNextPC = reg[15].I;
    reg[15].I += 4;
    ARM_PREFETCH;
    clockTicks = codeTicksAccessSeq32(armNextPC)*2 + codeTicksAccess32(armNextPC) + 3;
  }
}

// Load/store instructions ////////////////////////////////////////////////

// LDR R0~R7,[PC, #Imm]
static INSN_REGPARM void thumb48(u32 opcode)
{
  u8 regist = (opcode >> 8) & 7;
  UPDATE_BUS_PREFETCH
  u32 address = (reg[15].I & 0xFFFFFFFC) + ((opcode & 0xFF) << 2);
  reg[regist].I = CPUReadMemoryQuick(address);
  busPrefetchCount=0;
  clockTicks = 3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC);
}

// STR Rd, [Rs, Rn]
static INSN_REGPARM void thumb50(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
  CPUWriteMemory(address, reg[opcode & 7].I);
  clockTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
}

// STRH Rd, [Rs, Rn]
static INSN_REGPARM void thumb52(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
  CPUWriteHalfWord(address, reg[opcode&7].W.W0);
  clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
}

// STRB Rd, [Rs, Rn]
static INSN_REGPARM void thumb54(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode >>6)&7].I;
  CPUWriteByte(address, reg[opcode & 7].B.B0);
  clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
}

// LDSB Rd, [Rs, Rn]
static INSN_REGPARM void thumb56(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
  reg[opcode&7].I = (s8)CPUReadByte(address);
  clockTicks = 3 + dataTicksAccess16(address) + codeTicksAccess16(armNextPC);
}

// LDR Rd, [Rs, Rn]
static INSN_REGPARM void thumb58(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
  reg[opcode&7].I = CPUReadMemory(address);
  clockTicks = 3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC);
}

// LDRH Rd, [Rs, Rn]
static INSN_REGPARM void thumb5A(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
  reg[opcode&7].I = CPUReadHalfWord(address);
  clockTicks = 3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC);
}

// LDRB Rd, [Rs, Rn]
static INSN_REGPARM void thumb5C(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
  reg[opcode&7].I = CPUReadByte(address);
  clockTicks = 3 + dataTicksAccess16(address) + codeTicksAccess16(armNextPC);
}

// LDSH Rd, [Rs, Rn]
static INSN_REGPARM void thumb5E(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + reg[(opcode>>6)&7].I;
  reg[opcode&7].I = (u32)CPUReadHalfWordSigned(address);
  clockTicks = 3 + dataTicksAccess16(address) + codeTicksAccess16(armNextPC);
}

// STR Rd, [Rs, #Imm]
static INSN_REGPARM void thumb60(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<2);
  CPUWriteMemory(address, reg[opcode&7].I);
  clockTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
}

// LDR Rd, [Rs, #Imm]
static INSN_REGPARM void thumb68(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<2);
  reg[opcode&7].I = CPUReadMemory(address);
  clockTicks = 3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC);
}

// STRB Rd, [Rs, #Imm]
static INSN_REGPARM void thumb70(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31));
  CPUWriteByte(address, reg[opcode&7].B.B0);
  clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
}

// LDRB Rd, [Rs, #Imm]
static INSN_REGPARM void thumb78(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31));
  reg[opcode&7].I = CPUReadByte(address);
  clockTicks = 3 + dataTicksAccess16(address) + codeTicksAccess16(armNextPC);
}

// STRH Rd, [Rs, #Imm]
static INSN_REGPARM void thumb80(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<1);
  CPUWriteHalfWord(address, reg[opcode&7].W.W0);
  clockTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
}

// LDRH Rd, [Rs, #Imm]
static INSN_REGPARM void thumb88(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  u32 address = reg[(opcode>>3)&7].I + (((opcode>>6)&31)<<1);
  reg[opcode&7].I = CPUReadHalfWord(address);
  clockTicks = 3 + dataTicksAccess16(address) + codeTicksAccess16(armNextPC);
}

// STR R0~R7, [SP, #Imm]
static INSN_REGPARM void thumb90(u32 opcode)
{
  u8 regist = (opcode >> 8) & 7;
  UPDATE_BUS_PREFETCH
  u32 address = reg[13].I + ((opcode&255)<<2);
  CPUWriteMemory(address, reg[regist].I);
  clockTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
}

// LDR R0~R7, [SP, #Imm]
static INSN_REGPARM void thumb98(u32 opcode)
{
  u8 regist = (opcode >> 8) & 7;
  UPDATE_BUS_PREFETCH
  u32 address = reg[13].I + ((opcode&255)<<2);
  reg[regist].I = CPUReadMemoryQuick(address);
  clockTicks = 3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC);
}

// PC/stack-related ///////////////////////////////////////////////////////

// ADD R0~R7, PC, Imm
static INSN_REGPARM void thumbA0(u32 opcode)
{
  u8 regist = (opcode >> 8) & 7;
  reg[regist].I = (reg[15].I & 0xFFFFFFFC) + ((opcode&255)<<2);
  clockTicks = 1 + codeTicksAccess16(armNextPC);
}

// ADD R0~R7, SP, Imm
static INSN_REGPARM void thumbA8(u32 opcode)
{
  u8 regist = (opcode >> 8) & 7;
  reg[regist].I = reg[13].I + ((opcode&255)<<2);
  clockTicks = 1 + codeTicksAccess16(armNextPC);
}

// ADD SP, Imm
static INSN_REGPARM void thumbB0(u32 opcode)
{
  int offset = (opcode & 127) << 2;
  // Branchless negation: if bit 7 is set, mask becomes 0xFFFFFFFF, else 0x0
  u32 mask = -((opcode >> 7) & 1);
  // 2's complement branchless negation: (val ^ mask) - mask
  reg[13].I += (offset ^ mask) - mask;
  clockTicks = 1 + codeTicksAccess16(armNextPC);
}

// Push and pop ///////////////////////////////////////////////////////////

#define PUSH_REG(val, r)                                    \
  if (opcode & (val)) {                                     \
    CPUWriteMemory(address, reg[(r)].I);                    \
    u32 c = (u32)count;                                     \
    u32 seq = dataTicksAccessSeq32(address);                \
    u32 non = dataTicksAccess32(address);                   \
    /* Branchlessly select N-Cycle (count=0) or S-Cycle (count=1) */ \
    clockTicks += 1 + ((seq & -c) | (non & (c - 1)));       \
    count = 1;                                              \
    address += 4;                                           \
  }

#define POP_REG(val, r)                                     \
  if (opcode & (val)) {                                     \
    reg[(r)].I = CPUReadMemory(address);                    \
    u32 c = (u32)count;                                     \
    u32 seq = dataTicksAccessSeq32(address);                \
    u32 non = dataTicksAccess32(address);                   \
    clockTicks += 1 + ((seq & -c) | (non & (c - 1)));       \
    count = 1;                                              \
    address += 4;                                           \
  }

// PUSH {Rlist}
static INSN_REGPARM void thumbB4(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  int count = 0;
  u32 temp = reg[13].I - 4 * cpuBitsSet[opcode & 0xff];
  u32 address = temp & 0xFFFFFFFC;
  PUSH_REG(1, 0);
  PUSH_REG(2, 1);
  PUSH_REG(4, 2);
  PUSH_REG(8, 3);
  PUSH_REG(16, 4);
  PUSH_REG(32, 5);
  PUSH_REG(64, 6);
  PUSH_REG(128, 7);
  clockTicks += 1 + codeTicksAccess16(armNextPC);
  reg[13].I = temp;
}

// PUSH {Rlist, LR}
static INSN_REGPARM void thumbB5(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  int count = 0;
  u32 temp = reg[13].I - 4 - 4 * cpuBitsSet[opcode & 0xff];
  u32 address = temp & 0xFFFFFFFC;
  PUSH_REG(1, 0);
  PUSH_REG(2, 1);
  PUSH_REG(4, 2);
  PUSH_REG(8, 3);
  PUSH_REG(16, 4);
  PUSH_REG(32, 5);
  PUSH_REG(64, 6);
  PUSH_REG(128, 7);
  PUSH_REG(256, 14);
  clockTicks += 1 + codeTicksAccess16(armNextPC);
  reg[13].I = temp;
}

// POP {Rlist}
static INSN_REGPARM void thumbBC(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  int count = 0;
  u32 address = reg[13].I & 0xFFFFFFFC;
  u32 temp = reg[13].I + 4*cpuBitsSet[opcode & 0xFF];
  POP_REG(1, 0);
  POP_REG(2, 1);
  POP_REG(4, 2);
  POP_REG(8, 3);
  POP_REG(16, 4);
  POP_REG(32, 5);
  POP_REG(64, 6);
  POP_REG(128, 7);
  reg[13].I = temp;
  clockTicks = 2 + codeTicksAccess16(armNextPC);
}

// POP {Rlist, PC}
static INSN_REGPARM void thumbBD(u32 opcode)
{
  UPDATE_BUS_PREFETCH
  int count = 0;
  u32 address = reg[13].I & 0xFFFFFFFC;
  u32 temp = reg[13].I + 4 + 4*cpuBitsSet[opcode & 0xFF];
  POP_REG(1, 0);
  POP_REG(2, 1);
  POP_REG(4, 2);
  POP_REG(8, 3);
  POP_REG(16, 4);
  POP_REG(32, 5);
  POP_REG(64, 6);
  POP_REG(128, 7);
  reg[15].I = (CPUReadMemory(address) & 0xFFFFFFFE);
  if (!count) {
    clockTicks += 1 + dataTicksAccess32(address);
  } else {
    clockTicks += 1 + dataTicksAccessSeq32(address);
  }
  count++;
  armNextPC = reg[15].I;
  reg[15].I += 2;
  reg[13].I = temp;
  THUMB_PREFETCH;
  busPrefetchCount = 0;
  clockTicks += 3 + codeTicksAccess16(armNextPC) + codeTicksAccess16(armNextPC);
}

// Load/store multiple ////////////////////////////////////////////////////

#define THUMB_STM_REG(val,r,b)                              \
  if(opcode & (val)) {                                      \
    CPUWriteMemory(address, reg[(r)].I);                    \
    reg[(b)].I = temp;                                      \
    u32 c = (u32)count;                                     \
    u32 seq = dataTicksAccessSeq32(address);                \
    u32 non = dataTicksAccess32(address);                   \
    clockTicks += 1 + ((seq & -c) | (non & (c - 1)));       \
    count = 1;                                              \
    address += 4;                                           \
  }

#define THUMB_LDM_REG(val,r)                                \
  if(opcode & (val)) {                                      \
    reg[(r)].I = CPUReadMemory(address);                    \
    u32 c = (u32)count;                                     \
    u32 seq = dataTicksAccessSeq32(address);                \
    u32 non = dataTicksAccess32(address);                   \
    clockTicks += 1 + ((seq & -c) | (non & (c - 1)));       \
    count = 1;                                              \
    address += 4;                                           \
  }

// STM R0~7!, {Rlist}
static INSN_REGPARM void thumbC0(u32 opcode)
{
  u8 regist = (opcode >> 8) & 7;
  UPDATE_BUS_PREFETCH
  u32 address = reg[regist].I & 0xFFFFFFFC;
  u32 temp = reg[regist].I + 4*cpuBitsSet[opcode & 0xff];
  int count = 0;
  // store
  THUMB_STM_REG(1, 0, regist);
  THUMB_STM_REG(2, 1, regist);
  THUMB_STM_REG(4, 2, regist);
  THUMB_STM_REG(8, 3, regist);
  THUMB_STM_REG(16, 4, regist);
  THUMB_STM_REG(32, 5, regist);
  THUMB_STM_REG(64, 6, regist);
  THUMB_STM_REG(128, 7, regist);
  clockTicks = 1 + codeTicksAccess16(armNextPC);
}

// LDM R0~R7!, {Rlist}
static INSN_REGPARM void thumbC8(u32 opcode)
{
  u8 regist = (opcode >> 8) & 7;
  UPDATE_BUS_PREFETCH
  u32 address = reg[regist].I & 0xFFFFFFFC;
  u32 temp = reg[regist].I + 4*cpuBitsSet[opcode & 0xFF];
  int count = 0;
  // load
  THUMB_LDM_REG(1, 0);
  THUMB_LDM_REG(2, 1);
  THUMB_LDM_REG(4, 2);
  THUMB_LDM_REG(8, 3);
  THUMB_LDM_REG(16, 4);
  THUMB_LDM_REG(32, 5);
  THUMB_LDM_REG(64, 6);
  THUMB_LDM_REG(128, 7);
  clockTicks = 2 + codeTicksAccess16(armNextPC);
  if(!(opcode & (1<<regist)))
    reg[regist].I = temp;
}

// Conditional branches ///////////////////////////////////////////////////

// Broadway Optimization: Shared macro prevents I-Cache bloat from duplicated branch logic.
// It strictly restores the original clockTicks timing model which is critical for GBA sync.
#define THUMB_BRANCH_EXEC(cond) \
  clockTicks = codeTicksAccessSeq16(armNextPC) + 1; \
  if (cond) { \
    reg[15].I += ((s8)(opcode & 0xFF)) << 1; \
    armNextPC = reg[15].I; \
    reg[15].I += 2; \
    THUMB_PREFETCH; \
    clockTicks += codeTicksAccessSeq16(armNextPC) + codeTicksAccess16(armNextPC) + 2; \
    busPrefetchCount = 0; \
  }

// BEQ offset
static INSN_REGPARM void thumbD0(u32 opcode) { THUMB_BRANCH_EXEC(Z_FLAG); }
// BNE offset
static INSN_REGPARM void thumbD1(u32 opcode) { THUMB_BRANCH_EXEC(!Z_FLAG); }
// BCS offset
static INSN_REGPARM void thumbD2(u32 opcode) { THUMB_BRANCH_EXEC(C_FLAG); }
// BCC offset
static INSN_REGPARM void thumbD3(u32 opcode) { THUMB_BRANCH_EXEC(!C_FLAG); }
// BMI offset
static INSN_REGPARM void thumbD4(u32 opcode) { THUMB_BRANCH_EXEC(N_FLAG); }
// BPL offset
static INSN_REGPARM void thumbD5(u32 opcode) { THUMB_BRANCH_EXEC(!N_FLAG); }
// BVS offset
static INSN_REGPARM void thumbD6(u32 opcode) { THUMB_BRANCH_EXEC(V_FLAG); }
// BVC offset
static INSN_REGPARM void thumbD7(u32 opcode) { THUMB_BRANCH_EXEC(!V_FLAG); }
// BHI offset
static INSN_REGPARM void thumbD8(u32 opcode) { THUMB_BRANCH_EXEC(C_FLAG && !Z_FLAG); }
// BLS offset
static INSN_REGPARM void thumbD9(u32 opcode) { THUMB_BRANCH_EXEC(!C_FLAG || Z_FLAG); }
// BGE offset
static INSN_REGPARM void thumbDA(u32 opcode) { THUMB_BRANCH_EXEC(N_FLAG == V_FLAG); }
// BLT offset
static INSN_REGPARM void thumbDB(u32 opcode) { THUMB_BRANCH_EXEC(N_FLAG != V_FLAG); }
// BGT offset
static INSN_REGPARM void thumbDC(u32 opcode) { THUMB_BRANCH_EXEC(!Z_FLAG && (N_FLAG == V_FLAG)); }
// BLE offset
static INSN_REGPARM void thumbDD(u32 opcode) { THUMB_BRANCH_EXEC(Z_FLAG || (N_FLAG != V_FLAG)); }

// SWI #comment
static INSN_REGPARM void thumbDF(u32 opcode)
{
  u32 address = 0;
  clockTicks = 3;
  busPrefetchCount = 0;
  CPUSoftwareInterrupt(opcode & 0xFF);
}

// B offset
static INSN_REGPARM void thumbE0(u32 opcode)
{
  // Broadway Optimization: Branchless sign extension via arithmetic shift.
  // opcode bits 0-10 contain the 11-bit immediate.
  // Shifting left by 21 puts the 11th bit at the sign position (bit 31).
  // Shifting right (arithmetic) by 20 sign-extends it and multiplies it by 2 natively.
  s32 offset = ((s32)(opcode << 21)) >> 20;
  reg[15].I += offset;
  armNextPC = reg[15].I;
  reg[15].I += 2;
  THUMB_PREFETCH;
  clockTicks = codeTicksAccessSeq16(armNextPC)*2 + codeTicksAccess16(armNextPC)+3;
  busPrefetchCount=0;
}

// BLL #offset (forward)
static INSN_REGPARM void thumbF0(u32 opcode)
{
  int offset = (opcode & 0x7FF);
  reg[14].I = reg[15].I + (offset << 12);
  clockTicks = codeTicksAccessSeq16(armNextPC) + 1;
}

// BLL #offset (backward)
static INSN_REGPARM void thumbF4(u32 opcode)
{
  int offset = (opcode & 0x7FF);
  reg[14].I = reg[15].I + ((offset << 12) | 0xFF800000);
  clockTicks = codeTicksAccessSeq16(armNextPC) + 1;
}

// BLH #offset
static INSN_REGPARM void thumbF8(u32 opcode)
{
  int offset = (opcode & 0x7FF);
  u32 temp = reg[15].I - 2;
  reg[15].I = (reg[14].I + (offset << 1)) & 0xFFFFFFFE;
  armNextPC = reg[15].I;
  reg[15].I += 2;
  reg[14].I = temp | 1;
  THUMB_PREFETCH;
  clockTicks = codeTicksAccessSeq16(armNextPC)*2 + codeTicksAccess16(armNextPC) + 3;
  busPrefetchCount = 0;
}

// Instruction table //////////////////////////////////////////////////////

typedef INSN_REGPARM void (*insnfunc_t)(u32 opcode);
#define thumbUI thumbUnknownInsn
#define thumbBP thumbUnknownInsn
static insnfunc_t thumbInsnTable[1024] = {
  thumb00_00,thumb00_01,thumb00_02,thumb00_03,thumb00_04,thumb00_05,thumb00_06,thumb00_07,  // 00
  thumb00_08,thumb00_09,thumb00_0A,thumb00_0B,thumb00_0C,thumb00_0D,thumb00_0E,thumb00_0F,
  thumb00_10,thumb00_11,thumb00_12,thumb00_13,thumb00_14,thumb00_15,thumb00_16,thumb00_17,
  thumb00_18,thumb00_19,thumb00_1A,thumb00_1B,thumb00_1C,thumb00_1D,thumb00_1E,thumb00_1F,
  thumb08_00,thumb08_01,thumb08_02,thumb08_03,thumb08_04,thumb08_05,thumb08_06,thumb08_07,  // 08
  thumb08_08,thumb08_09,thumb08_0A,thumb08_0B,thumb08_0C,thumb08_0D,thumb08_0E,thumb08_0F,
  thumb08_10,thumb08_11,thumb08_12,thumb08_13,thumb08_14,thumb08_15,thumb08_16,thumb08_17,
  thumb08_18,thumb08_19,thumb08_1A,thumb08_1B,thumb08_1C,thumb08_1D,thumb08_1E,thumb08_1F,
  thumb10_00,thumb10_01,thumb10_02,thumb10_03,thumb10_04,thumb10_05,thumb10_06,thumb10_07,  // 10
  thumb10_08,thumb10_09,thumb10_0A,thumb10_0B,thumb10_0C,thumb10_0D,thumb10_0E,thumb10_0F,
  thumb10_10,thumb10_11,thumb10_12,thumb10_13,thumb10_14,thumb10_15,thumb10_16,thumb10_17,
  thumb10_18,thumb10_19,thumb10_1A,thumb10_1B,thumb10_1C,thumb10_1D,thumb10_1E,thumb10_1F,
  thumb18_0,thumb18_1,thumb18_2,thumb18_3,thumb18_4,thumb18_5,thumb18_6,thumb18_7,          // 18
  thumb1A_0,thumb1A_1,thumb1A_2,thumb1A_3,thumb1A_4,thumb1A_5,thumb1A_6,thumb1A_7,
  thumb1C_0,thumb1C_1,thumb1C_2,thumb1C_3,thumb1C_4,thumb1C_5,thumb1C_6,thumb1C_7,
  thumb1E_0,thumb1E_1,thumb1E_2,thumb1E_3,thumb1E_4,thumb1E_5,thumb1E_6,thumb1E_7,
  thumb20,thumb20,thumb20,thumb20,thumb21,thumb21,thumb21,thumb21,  // 20
  thumb22,thumb22,thumb22,thumb22,thumb23,thumb23,thumb23,thumb23,
  thumb24,thumb24,thumb24,thumb24,thumb25,thumb25,thumb25,thumb25,
  thumb26,thumb26,thumb26,thumb26,thumb27,thumb27,thumb27,thumb27,
  thumb28,thumb28,thumb28,thumb28,thumb29,thumb29,thumb29,thumb29,  // 28
  thumb2A,thumb2A,thumb2A,thumb2A,thumb2B,thumb2B,thumb2B,thumb2B,
  thumb2C,thumb2C,thumb2C,thumb2C,thumb2D,thumb2D,thumb2D,thumb2D,
  thumb2E,thumb2E,thumb2E,thumb2E,thumb2F,thumb2F,thumb2F,thumb2F,
  thumb30,thumb30,thumb30,thumb30,thumb31,thumb31,thumb31,thumb31,  // 30
  thumb32,thumb32,thumb32,thumb32,thumb33,thumb33,thumb33,thumb33,
  thumb34,thumb34,thumb34,thumb34,thumb35,thumb35,thumb35,thumb35,
  thumb36,thumb36,thumb36,thumb36,thumb37,thumb37,thumb37,thumb37,
  thumb38,thumb38,thumb38,thumb38,thumb39,thumb39,thumb39,thumb39,  // 38
  thumb3A,thumb3A,thumb3A,thumb3A,thumb3B,thumb3B,thumb3B,thumb3B,
  thumb3C,thumb3C,thumb3C,thumb3C,thumb3D,thumb3D,thumb3D,thumb3D,
  thumb3E,thumb3E,thumb3E,thumb3E,thumb3F,thumb3F,thumb3F,thumb3F,
  thumb40_0,thumb40_1,thumb40_2,thumb40_3,thumb41_0,thumb41_1,thumb41_2,thumb41_3,  // 40
  thumb42_0,thumb42_1,thumb42_2,thumb42_3,thumb43_0,thumb43_1,thumb43_2,thumb43_3,
  thumbUI,thumb44_1,thumb44_2,thumb44_3,thumbUI,thumb45_1,thumb45_2,thumb45_3,
  thumb46_0,thumb46_1,thumb46_2,thumb46_3,thumb47,thumb47,thumbUI,thumbUI,
  thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,  // 48
  thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,
  thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,
  thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,thumb48,
  thumb50,thumb50,thumb50,thumb50,thumb50,thumb50,thumb50,thumb50,  // 50
  thumb52,thumb52,thumb52,thumb52,thumb52,thumb52,thumb52,thumb52,
  thumb54,thumb54,thumb54,thumb54,thumb54,thumb54,thumb54,thumb54,
  thumb56,thumb56,thumb56,thumb56,thumb56,thumb56,thumb56,thumb56,
  thumb58,thumb58,thumb58,thumb58,thumb58,thumb58,thumb58,thumb58,  // 58
  thumb5A,thumb5A,thumb5A,thumb5A,thumb5A,thumb5A,thumb5A,thumb5A,
  thumb5C,thumb5C,thumb5C,thumb5C,thumb5C,thumb5C,thumb5C,thumb5C,
  thumb5E,thumb5E,thumb5E,thumb5E,thumb5E,thumb5E,thumb5E,thumb5E,
  thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,  // 60
  thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,
  thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,
  thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,thumb60,
  thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,  // 68
  thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,
  thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,
  thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,thumb68,
  thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,  // 70
  thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,
  thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,
  thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,thumb70,
  thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,  // 78
  thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,
  thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,
  thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,thumb78,
  thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,  // 80
  thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,
  thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,
  thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,thumb80,
  thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,  // 88
  thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,
  thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,
  thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,thumb88,
  thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,  // 90
  thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,
  thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,
  thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,thumb90,
  thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,  // 98
  thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,
  thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,
  thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,thumb98,
  thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,  // A0
  thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,
  thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,
  thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,thumbA0,
  thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,  // A8
  thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,
  thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,
  thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,thumbA8,
  thumbB0,thumbB0,thumbB0,thumbB0,thumbUI,thumbUI,thumbUI,thumbUI,  // B0
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,
  thumbB4,thumbB4,thumbB4,thumbB4,thumbB5,thumbB5,thumbB5,thumbB5,
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,  // B8
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,
  thumbBC,thumbBC,thumbBC,thumbBC,thumbBD,thumbBD,thumbBD,thumbBD,
  thumbBP,thumbBP,thumbBP,thumbBP,thumbUI,thumbUI,thumbUI,thumbUI,
  thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,  // C0
  thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,
  thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,
  thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,thumbC0,
  thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,  // C8
  thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,
  thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,
  thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,thumbC8,
  thumbD0,thumbD0,thumbD0,thumbD0,thumbD1,thumbD1,thumbD1,thumbD1,  // D0
  thumbD2,thumbD2,thumbD2,thumbD2,thumbD3,thumbD3,thumbD3,thumbD3,
  thumbD4,thumbD4,thumbD4,thumbD4,thumbD5,thumbD5,thumbD5,thumbD5,
  thumbD6,thumbD6,thumbD6,thumbD6,thumbD7,thumbD7,thumbD7,thumbD7,
  thumbD8,thumbD8,thumbD8,thumbD8,thumbD9,thumbD9,thumbD9,thumbD9,  // D8
  thumbDA,thumbDA,thumbDA,thumbDA,thumbDB,thumbDB,thumbDB,thumbDB,
  thumbDC,thumbDC,thumbDC,thumbDC,thumbDD,thumbDD,thumbDD,thumbDD,
  thumbUI,thumbUI,thumbUI,thumbUI,thumbDF,thumbDF,thumbDF,thumbDF,
  thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,  // E0
  thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,
  thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,
  thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,thumbE0,
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,  // E8
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,
  thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,thumbUI,
  thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,  // F0
  thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,thumbF0,
  thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,
  thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,thumbF4,
  thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,  // F8
  thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,
  thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,
  thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,thumbF8,
};
#ifdef JIT_COMPILER_DIFFERENTIAL_TESTING

// -------------------------------------------------------------------------
// HYBRID TRACE-JIT / C++ EXECUTION ENGINE DIFFERENTIAL TESTING
// -------------------------------------------------------------------------
int thumbExecute() {
    do {
        u32 pc = armNextPC;
        BasicBlock* block = jitCache.getBlock(pc);

        if (__builtin_expect(block == nullptr, 0)) {
            block = JITCompileThumbTrace(pc, jitCache);
        }

        // ========================================================================
        // JIT EXECUTION PATH
        // ========================================================================
        if (block != nullptr && block->execute != nullptr) {
            if (g_jitStats.mismatchCount < MAX_JIT_MISMATCH_COUNT) {
                // 1. SAVE CPU STATE BEFORE JIT TRACE
                u32 savedRegs[16];
                for (size_t i = 0; i < 16; i++) savedRegs[i] = reg[i].I;
                u32 savedN = N_FLAG, savedZ = Z_FLAG, savedC = C_FLAG, savedV = V_FLAG;

                // 2. RUN JIT EXECUTION
                u32 jitFlags[4] = { (u32)N_FLAG, (u32)Z_FLAG, (u32)C_FLAG, (u32)V_FLAG };
                JITResult jitResult;

                reg[15].I = pc + 4;
                ExecuteJITTrace(block->execute, (u32*)(void*)&reg[0].I, jitFlags, &jitResult, gbaReadPagePtrs, gbaReadPageMasks);

                JIT_LOG_EXEC(block->length);

                u32 jitRegs[16];
                for (size_t i = 0; i < 16; i++) jitRegs[i] = reg[i].I;
                u32 jitN = jitFlags[0], jitZ = jitFlags[1], jitC = jitFlags[2], jitV = jitFlags[3];

                // 3. REWIND CPU STATE FOR C++ INTERPRETER
                for (size_t i = 0; i < 16; i++) reg[i].I = savedRegs[i];
                N_FLAG = savedN; Z_FLAG = savedZ; C_FLAG = savedC; V_FLAG = savedV;

                // 4. RUN C++ INTERPRETER UNTIL IT REACHES THE SAME PC THE JIT REACHED
                u32 interpPC = pc;
                int cppCycles = 0;
                const u32 kMaxSteps = 256; // safety cap — real divergence, not infinite loop
                u32 steps = 0;

                while (interpPC != jitResult.nextPC && steps < kMaxSteps) {
                    u16 opcode = CPUReadHalfWord(interpPC);
                    armNextPC = interpPC + 2;
                    reg[15].I = interpPC + 4;
                    clockTicks = 0;
                    (*thumbInsnTable[opcode >> 6])(opcode);
                    int localTicks = clockTicks;
                    if (localTicks <= 0) localTicks = codeTicksAccessSeq16(interpPC) + 1;
                    cppCycles += localTicks;
                    interpPC = armNextPC;
                    steps++;
                }

                // 5. DETECT & COMPARE ALL CPU FIELDS (BEFORE & AFTER)
                bool mismatch = false;
                bool regMismatches[15] = { false };
                bool pcMismatch = false;
                bool flagMismatch = false;
                bool cycleMismatch = false;

                // Compare R0 - R14 (R13=SP, R14=LR)
                for (int i = 0; i < 15; i++) {
                    if (jitRegs[i] != reg[i].I) {
                        regMismatches[i] = true;
                        mismatch = true;
                    }
                }

                // Compare Next PC
                if (jitResult.nextPC != interpPC) {
                    pcMismatch = true;
                    mismatch = true;
                }

                // Compare N, Z, C, V Flags
                if (jitN != N_FLAG || jitZ != Z_FLAG || jitC != C_FLAG || jitV != V_FLAG) {
                    flagMismatch = true;
                    mismatch = true;
                }

                // Compare Cycle Timing
                if ((int)jitResult.cycles != cppCycles) {
                    cycleMismatch = true;
                    mismatch = true;
                }

                // 6. LOG DETAILED MISMATCH REPORT IF DISCREPANCY FOUND
                if (mismatch) {
                    static const char* regNames[15] = {
                        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
                        "R8", "R9", "R10", "R11", "R12", "SP", "LR"
                    };
                    u16 startOpcode = CPUReadHalfWord(pc);

                    static std::string assembledMsg;
                    static char tempBuf[1024];

                    auto appendToMsg = [&](const char* format, ...) {
						va_list args;
						va_start(args, format);
						vsnprintf(tempBuf, sizeof(tempBuf), format, args);
						va_end(args);
						assembledMsg.append(tempBuf);
					};

					appendToMsg("StartPC: 0x%08X | Trace Length: %u | Opcode: 0x%04X\n", pc, block->length, startOpcode);
					appendToMsg("Initial Flags: N=%u Z=%u C=%u V=%u\n", savedN, savedZ, savedC, savedV);
					appendToMsg("JIT Result:  NextPC=0x%08X | Cycles=%u | Flags=(N:%u Z:%u C:%u V:%u)\n",
                        jitResult.nextPC, jitResult.cycles, jitN, jitZ, jitC, jitV);
					appendToMsg("C++ Result:  NextPC=0x%08X | Cycles=%d | Flags=(N:%u Z:%u C:%u V:%u)\n",
                        interpPC, cppCycles, N_FLAG, Z_FLAG, C_FLAG, V_FLAG);

					appendToMsg("--- MISMATCH DETAILS ---\n");
                    for (int i = 0; i < 15; i++) {
                        if (regMismatches[i]) {
                        	appendToMsg("  [REG %-3s] Initial=0x%08X | JIT=0x%08X vs C++=0x%08X\n",
                                regNames[i], savedRegs[i], jitRegs[i], reg[i].I);
                        }
                    }
                    if (pcMismatch) {
                    	appendToMsg("  [NEXT PC] JIT=0x%08X vs C++=0x%08X\n", jitResult.nextPC, interpPC);
                    }
                    if (flagMismatch) {
                    	appendToMsg("  [FLAGS]   JIT=(N:%u Z:%u C:%u V:%u) vs C++=(N:%u Z:%u C:%u V:%u)\n",
                            jitN, jitZ, jitC, jitV, N_FLAG, Z_FLAG, C_FLAG, V_FLAG);
                    }
                    if (cycleMismatch) {
                    	appendToMsg("  [CYCLES]  JIT=%u vs C++=%d\n", jitResult.cycles, cppCycles);
                    }

                    JIT_LOG_MISMATCH(assembledMsg.c_str());

                    // RESTORE C++ STATE AS GROUND TRUTH
                    armNextPC = interpPC;
                    cpuTotalTicks += cppCycles;
                    return 1;
                } else {
                    // SUCCESSFUL MATCH: Commit JIT state and return immediately (prevents double execution)
                    N_FLAG = jitN; Z_FLAG = jitZ; C_FLAG = jitC; V_FLAG = jitV;
                    for (size_t i = 0; i < 15; i++) reg[i].I = jitRegs[i];
                    armNextPC = jitResult.nextPC;
                    reg[15].I = armNextPC + 2;
                    cpuTotalTicks += jitResult.cycles;
                    return 1;
                }
            }

            u32 flagBuffer[4] = { (u32)N_FLAG, (u32)Z_FLAG, (u32)C_FLAG, (u32)V_FLAG };
			JITResult result;

			// Align reg[15].I to pc + 4 so that PC-relative reads
			// inside the compiled JIT trace match authentic GBA pipeline values.
			reg[15].I = pc + 4;

			JIT_LOG_TRACE_ENTRY(pc, flagBuffer);

			// Execute Native Trace with flat memory maps
			ExecuteJITTrace(block->execute, (u32*)&reg[0].I, flagBuffer, &result, gbaReadPagePtrs, gbaReadPageMasks);

			JIT_LOG_TRACE_EXIT(pc, result.nextPC, flagBuffer, result.cycles);

			JIT_LOG_EXEC(block->length);

			// Restore updated status flags
			N_FLAG = flagBuffer[0];
			Z_FLAG = flagBuffer[1];
			C_FLAG = flagBuffer[2];
			V_FLAG = flagBuffer[3];

			// Account for execution cycles accumulated by the trace block
			cpuTotalTicks += result.cycles;

			// PERFECT PIPELINE RE-PRIME
			// When returning from the JIT, the GBA hardware prefetcher is broken.
			// We MUST reset busPrefetchCount or the C++ fallback will falsely charge 0 cycles.
			busPrefetchCount = 0;

			armNextPC = result.nextPC;
			reg[15].I = armNextPC + 2;
			cpuPrefetch[0] = CPUReadHalfWord(armNextPC);
			cpuPrefetch[1] = CPUReadHalfWord(armNextPC + 2);
			continue;
        }

        // FALLBACK TO C++ INTERPRETER
        u16 opcode = cpuPrefetch[0];
		cpuPrefetch[0] = cpuPrefetch[1];

		JIT_LOG_FALLBACK(opcode);

		busPrefetch = false;
		if (busPrefetchCount & 0xFFFFFF00)
			busPrefetchCount = 0x100 | (busPrefetchCount & 0xFF);

		clockTicks = 0;
		u32 oldArmNextPC = armNextPC;
		armNextPC = reg[15].I;
		reg[15].I += 2;

		THUMB_PREFETCH_NEXT;

		clockTicks = 0;
		(*thumbInsnTable[opcode>>6])(opcode);
		int localTicks = clockTicks;

		if (localTicks < 0) return 0;
		if (localTicks == 0) localTicks = codeTicksAccessSeq16(oldArmNextPC) + 1;

		cpuTotalTicks += localTicks;
    } while (cpuTotalTicks < cpuNextEvent && !armState && !holdState && !SWITicks);
    return 1;
}
#elif JIT_COMPILER
// -------------------------------------------------------------------------
// HYBRID TRACE-JIT / C++ EXECUTION ENGINE
// -------------------------------------------------------------------------
int thumbExecute() {
    do {
        u32 pc = armNextPC;
        BasicBlock* block = jitCache.getBlock(pc);

        if (__builtin_expect(block == nullptr, 0)) {
            block = JITCompileThumbTrace(pc, jitCache);
        }

        // ========================================================================
		// JIT EXECUTION PATH
		// ========================================================================
		if (block != nullptr && block->execute != nullptr) {
			u32 flagBuffer[4] = { (u32)N_FLAG, (u32)Z_FLAG, (u32)C_FLAG, (u32)V_FLAG };
			JITResult result;

			// Align reg[15].I to pc + 4 so that PC-relative reads
			// inside the compiled JIT trace match authentic GBA pipeline values.
			reg[15].I = pc + 4;

			JIT_LOG_TRACE_ENTRY(pc, flagBuffer);

			// Execute Native Trace with flat memory maps
			ExecuteJITTrace(block->execute, (u32*)&reg[0].I, flagBuffer, &result, gbaReadPagePtrs, gbaReadPageMasks);

			JIT_LOG_TRACE_EXIT(pc, result.nextPC, flagBuffer, result.cycles);

			JIT_LOG_EXEC(block->length);

			// Restore updated status flags
			N_FLAG = flagBuffer[0];
			Z_FLAG = flagBuffer[1];
			C_FLAG = flagBuffer[2];
			V_FLAG = flagBuffer[3];

			// Account for execution cycles accumulated by the trace block
			cpuTotalTicks += result.cycles;

			// PERFECT PIPELINE RE-PRIME
			// When returning from the JIT, the GBA hardware prefetcher is broken.
			// We MUST reset busPrefetchCount or the C++ fallback will falsely charge 0 cycles.
			busPrefetchCount = 0;

			armNextPC = result.nextPC;
			reg[15].I = armNextPC + 2;
			cpuPrefetch[0] = CPUReadHalfWord(armNextPC);
			cpuPrefetch[1] = CPUReadHalfWord(armNextPC + 2);
			continue;
		}

        // ========================================================================
        // LEGACY C++ FALLBACK PATH
        // ========================================================================
        if (cheatsEnabled) cpuMasterCodeCheck();

        u16 opcode = cpuPrefetch[0];
        cpuPrefetch[0] = cpuPrefetch[1];

        JIT_LOG_FALLBACK(opcode);

        busPrefetch = false;
        if (busPrefetchCount & 0xFFFFFF00)
            busPrefetchCount = 0x100 | (busPrefetchCount & 0xFF);

        clockTicks = 0;
        u32 oldArmNextPC = armNextPC;
        armNextPC = reg[15].I;
        reg[15].I += 2;

        THUMB_PREFETCH_NEXT;

        // --- OPTIMIZED INLINE DISPATCH FOR HIGH-FREQUENCY INSTRUCTIONS ---
        bool handledInline = false;

        // 1. THUMB Format 3: Move Immediate (MOV Rd, #Imm8)
        if ((opcode & 0xF800) == 0x2000) {
            u8 rd = (opcode >> 8) & 0x07;
            u8 imm = opcode & 0xFF;

            reg[rd].I = imm;
            N_FLAG = 0;
            Z_FLAG = (imm == 0);

            int localTicks = codeTicksAccessSeq16(oldArmNextPC) + 1;
            cpuTotalTicks += localTicks;
            handledInline = true;
        }

        // ========================================================================
        // FALLBACK: Instruction jump table
        // ========================================================================
        if (!handledInline) {
            clockTicks = 0;
            (*thumbInsnTable[opcode>>6])(opcode);
            int localTicks = clockTicks;

            if (localTicks < 0) return 0;
            if (localTicks == 0) localTicks = codeTicksAccessSeq16(oldArmNextPC) + 1;

            cpuTotalTicks += localTicks;
        }
    } while (cpuTotalTicks < cpuNextEvent && !armState && !holdState && !SWITicks);

    return 1;
}
#else
// Wrapper routine (execution loop) ///////////////////////////////////////

// ========================================================================
// OPTIMIZED THUMB EXECUTION LOOP (FAST-PATH DISPATCHER)
// Replaces jump-table 'switch' with predictable 'if/else' range cascades.
// Eliminates Function Call ABI overhead and CTR indirect branch stalling.
// Localizes the clockTicks state to prevent global register spilling.
// ========================================================================

int thumbExecute()
{
    do {
        if( cheatsEnabled ) {
            cpuMasterCodeCheck();
        }

        u32 opcode = cpuPrefetch[0];
        cpuPrefetch[0] = cpuPrefetch[1];

        // Branchless bus prefetch boundary check
        busPrefetch = false;
        if (busPrefetchCount & 0xFFFFFF00)
            busPrefetchCount = 0x100 | (busPrefetchCount & 0xFF);

        u32 oldArmNextPC = armNextPC;
        armNextPC = reg[15].I;
        reg[15].I += 2;

		// --- STREAMLINED O(1) INSTRUCTION PREFETCH ---
		// Automatically handles the +2 pipeline offset and issues the native L1 dcbt
        THUMB_PREFETCH_NEXT;

		// --- OPTIONAL FAST-PATH DISPATCHER ---
		// Handle the most common opcodes inline to avoid the thumbInsnTable overhead
        int localTicks = 0;
        bool handledInline = false;

		// FAST-PATH DISPATCHER
		// Extract top 5 bits (opcode >> 11) to intercept ALU/Shift/Immediate ops
        int top5 = opcode >> 11;

        // ========================================================================
        // FAST-PATH 1: ALU, SHIFTS, AND IMMEDIATES (Opcodes 0x0000 - 0x3FFF)
        // Grouped to force a single 'cmpwi' branch evaluation.
        // ========================================================================
        if (top5 <= 7) {
            if (top5 == 0) { // LSL Rd, Rm, #Imm5 (thumb00)
                int dest = opcode & 0x07;
                int source = (opcode >> 3) & 0x07;
                int shift = (opcode >> 6) & 0x1F;
                u32 src_val = reg[source].I;

				// Branchless mask: 0xFFFFFFFF if shift > 0, 0x00000000 if shift == 0
                u32 shift_mask = -(u32)(shift != 0);

				// If shift==0, C_FLAG is untouched. If shift>0, C_FLAG = bit (32-shift).
                u32 new_c = (src_val >> ((32 - shift) & 0x1F)) & 1;
                C_FLAG = (new_c & shift_mask) | (C_FLAG & ~shift_mask);

				// Branchless value calculation
                u32 value = ((src_val << shift) & shift_mask) | (src_val & ~shift_mask);
                reg[dest].I = value;
				N_FLAG = (value >> 31);
				Z_FLAG = (value == 0);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 1) { // LSR Rd, Rm, #Imm5 (thumb08)
                int dest = opcode & 0x07;
                int source = (opcode >> 3) & 0x07;
                int shift = (opcode >> 6) & 0x1F;
                u32 src_val = reg[source].I;

                u32 shift_mask = -(u32)(shift != 0);

				// If shift != 0, C is bit (shift-1). If shift == 0, C is bit 31.
                u32 c_shift_nz = (shift - 1) & 0x1F;
                u32 new_c = ((src_val >> c_shift_nz) & shift_mask) | ((src_val >> 31) & ~shift_mask);
                C_FLAG = new_c & 1;

				// If shift==0, value is 0.
                u32 value = (src_val >> shift) & shift_mask;
                reg[dest].I = value;
				N_FLAG = (value >> 31);
				Z_FLAG = (value == 0);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 2) { // ASR Rd, Rm, #Imm5 (thumb10)
                int dest = opcode & 0x07;
                int source = (opcode >> 3) & 0x07;
                int shift = (opcode >> 6) & 0x1F;
                s32 src_val = (s32)reg[source].I;

                u32 shift_mask = -(u32)(shift != 0);
                u32 c_shift_nz = (shift - 1) & 0x1F;
                u32 new_c = (((u32)src_val >> c_shift_nz) & shift_mask) | (((u32)src_val >> 31) & ~shift_mask);
                C_FLAG = new_c & 1;

                u32 sign_ext = -((u32)((u32)src_val >> 31));
                u32 value = (((u32)(src_val >> shift)) & shift_mask) | (sign_ext & ~shift_mask);
                reg[dest].I = value;
				N_FLAG = (value >> 31);
				Z_FLAG = (value == 0);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 3) { // ADD/SUB Rd, Rs, Rn / #Imm3 (thumb18 - thumb1E)
                int dest = opcode & 0x07;
                int source = (opcode >> 3) & 0x07;
                u32 lhs = reg[source].I;

				// Bit 10 distinguishes between immediate (1) and register (0)
                u32 rhs = (opcode & 0x0400) ? ((opcode >> 6) & 0x07) : reg[(opcode >> 6) & 0x07].I;
                u32 res;

				// Bit 9 distinguishes between SUB (1) and ADD (0)
                if (opcode & 0x0200) {
                    res = lhs - rhs;
					SUBCARRY(lhs, rhs, res);
					SUBOVERFLOW(lhs, rhs, res);
                } else {
                    res = lhs + rhs;
					ADDCARRY(lhs, rhs, res);
					ADDOVERFLOW(lhs, rhs, res);
                }
                reg[dest].I = res;
				Z_FLAG = (res == 0);
				N_FLAG = (res >> 31);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 4) { // MOV Rd, #Imm8 (thumb20)
                int dest = (opcode >> 8) & 0x07;
                u32 value = opcode & 0xFF;
                reg[dest].I = value;
				N_FLAG = 0;
				Z_FLAG = (value == 0);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 5) { // CMP Rd, #Imm8 (thumb28)
                int dest = (opcode >> 8) & 0x07;
                u32 lhs = reg[dest].I;
                u32 rhs = opcode & 0xFF;
                u32 res = lhs - rhs;
				Z_FLAG = (res == 0);
				N_FLAG = (res >> 31);
				SUBCARRY(lhs, rhs, res);
				SUBOVERFLOW(lhs, rhs, res);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 6) { // ADD Rd, #Imm8 (thumb30)
                int dest = (opcode >> 8) & 0x07;
                u32 lhs = reg[dest].I;
                u32 rhs = opcode & 0xFF;
                u32 res = lhs + rhs;
                reg[dest].I = res;
				Z_FLAG = (res == 0);
				N_FLAG = (res >> 31);
				ADDCARRY(lhs, rhs, res);
				ADDOVERFLOW(lhs, rhs, res);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 7) { // SUB Rd, #Imm8 (thumb38)
                int dest = (opcode >> 8) & 0x07;
                u32 lhs = reg[dest].I;
                u32 rhs = opcode & 0xFF;
                u32 res = lhs - rhs;
                reg[dest].I = res;
				Z_FLAG = (res == 0);
				N_FLAG = (res >> 31);
				SUBCARRY(lhs, rhs, res);
				SUBOVERFLOW(lhs, rhs, res);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
        }
        // ========================================================================
        // FAST-PATH 2: MEMORY I/O (Opcodes 0x6000 - 0x9FFF)
        // Integrates completely with our O(1) Memory Pages
        // ========================================================================
        else if (top5 >= 12 && top5 <= 19) {
            busPrefetch |= (busPrefetchEnable & (busPrefetchCount == 0));

            if (top5 == 12) { // STR Rd, [Rb, #Imm] (thumb60)
                int dest = opcode & 0x07;
                int base = (opcode >> 3) & 0x07;
                u32 address = reg[base].I + (((opcode >> 6) & 0x1F) << 2);
                CPUWriteMemory(address, reg[dest].I);
                localTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
                handledInline = true;
            }
            else if (top5 == 13) { // LDR Rd, [Rb, #Imm] (thumb68)
                int dest = opcode & 0x07;
                int base = (opcode >> 3) & 0x07;
                u32 address = reg[base].I + (((opcode >> 6) & 0x1F) << 2);
                reg[dest].I = CPUReadMemory(address);
                localTicks = 3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC);
                handledInline = true;
            }
            else if (top5 == 14) { // STRB Rd, [Rb, #Imm] (thumb70)
                int dest = opcode & 0x07;
                int base = (opcode >> 3) & 0x07;
                u32 address = reg[base].I + ((opcode >> 6) & 0x1F);
                CPUWriteByte(address, reg[dest].B.B0);
                localTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
                handledInline = true;
            }
            else if (top5 == 15) { // LDRB Rd, [Rb, #Imm] (thumb78)
                int dest = opcode & 0x07;
                int base = (opcode >> 3) & 0x07;
                u32 address = reg[base].I + ((opcode >> 6) & 0x1F);
                reg[dest].I = CPUReadByte(address);
                localTicks = 3 + dataTicksAccess16(address) + codeTicksAccess16(armNextPC);
                handledInline = true;
            }
            else if (top5 == 16) { // STRH Rd, [Rb, #Imm] (thumb80)
                int dest = opcode & 0x07;
                int base = (opcode >> 3) & 0x07;
                u32 address = reg[base].I + (((opcode >> 6) & 0x1F) << 1);
                CPUWriteHalfWord(address, reg[dest].W.W0);
                localTicks = dataTicksAccess16(address) + codeTicksAccess16(armNextPC) + 2;
                handledInline = true;
            }
            else if (top5 == 17) { // LDRH Rd, [Rb, #Imm] (thumb80)
                int dest = opcode & 0x07;
                int base = (opcode >> 3) & 0x07;
                u32 address = reg[base].I + (((opcode >> 6) & 0x1F) << 1);
                reg[dest].I = CPUReadHalfWord(address);
                localTicks = 3 + dataTicksAccess16(address) + codeTicksAccess16(armNextPC);
                handledInline = true;
            }
            else if (top5 == 18) { // STR R0~R7, [SP, #Imm8] (thumb90)
                int dest = (opcode >> 8) & 0x07;
                u32 address = reg[13].I + ((opcode & 0xFF) << 2);
                CPUWriteMemory(address, reg[dest].I);
                localTicks = dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2;
                handledInline = true;
            }
            else if (top5 == 19) { // LDR R0~R7, [SP, #Imm8] (thumb98)
                int dest = (opcode >> 8) & 0x07;
                u32 address = reg[13].I + ((opcode & 0xFF) << 2);
                reg[dest].I = CPUReadMemoryQuick(address); // Aligns perfectly to stack RAM
                localTicks = 3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC);
                handledInline = true;
            }
        }
        // ========================================================================
        // FAST-PATH 3: UNCONDITIONAL BRANCHING (Opcodes 0xE000 - 0xFFFF)
        // Intercepts opcodes 0xE and 0xF to bypass the 1,024-entry indirect table.
		// Uses 1-cycle hardware arithmetic shifts to handle sign-extension branchlessly.
        // ========================================================================
        else if (top5 >= 28) {
            if (top5 == 28) { // B offset (thumbE0)
            	// Branchless 11-bit sign extension and multiply-by-two
                s32 offset = ((s32)(opcode << 21)) >> 20;
                reg[15].I += offset;
                armNextPC = reg[15].I;
                reg[15].I += 2;
                THUMB_PREFETCH; // Evaluates natively via the updated macro
                localTicks = codeTicksAccessSeq16(armNextPC) * 2 + codeTicksAccess16(armNextPC) + 3;
                busPrefetchCount = 0;
                handledInline = true;
            }
            else if (top5 == 30) { // BL prefix (thumbF0 and thumbF4)
            	// Broadway Optimization: Branchless sign extension of the 11-bit offset.
				// opcode << 21 places the 11th bit (the sign bit) directly at the MSB (bit 31).
				// Arithmetic shift right by 9 sign-extends it and aligns it to a 12-bit left shift natively.
				// Replaces conditional 'if (opcode & 0x0400)' entirely.
                reg[14].I = reg[15].I + (((s32)(opcode << 21)) >> 9);
                localTicks = codeTicksAccessSeq16(armNextPC) + 1;
                handledInline = true;
            }
            else if (top5 == 31) { // BL/BLX suffix (thumbF8)
                u32 temp = reg[15].I - 2;
                reg[15].I = (reg[14].I + ((opcode & 0x7FF) << 1)) & 0xFFFFFFFE;
                armNextPC = reg[15].I;
                reg[15].I += 2;
                reg[14].I = temp | 1;
                THUMB_PREFETCH;
                localTicks = codeTicksAccessSeq16(armNextPC) * 2 + codeTicksAccess16(armNextPC) + 3;
                busPrefetchCount = 0;
                handledInline = true;
            }
        }
        // ========================================================================
        // FAST-PATH 4: CONDITIONAL BRANCHES (Bcc - 0xD000 - 0xDDFF)
        // ========================================================================
        else if ((opcode >> 12) == 0xD && ((opcode >> 8) & 0xF) < 0xE) {
            int cond = 0;
            switch ((opcode >> 8) & 0xF) {
                case 0x0: cond = Z_FLAG; break;
                case 0x1: cond = !Z_FLAG; break;
                case 0x2: cond = C_FLAG; break;
                case 0x3: cond = !C_FLAG; break;
                case 0x4: cond = N_FLAG; break;
                case 0x5: cond = !N_FLAG; break;
                case 0x6: cond = V_FLAG; break;
                case 0x7: cond = !V_FLAG; break;
                case 0x8: cond = C_FLAG && !Z_FLAG; break;
                case 0x9: cond = !C_FLAG || Z_FLAG; break;
                case 0xA: cond = N_FLAG == V_FLAG; break;
                case 0xB: cond = N_FLAG != V_FLAG; break;
                case 0xC: cond = !Z_FLAG && (N_FLAG == V_FLAG); break;
                case 0xD: cond = Z_FLAG || (N_FLAG != V_FLAG); break;
            }

            localTicks = codeTicksAccessSeq16(armNextPC) + 1;

            if (cond) {
                reg[15].I += ((s8)(opcode & 0xFF)) << 1;
                armNextPC = reg[15].I;
                reg[15].I += 2;
                THUMB_PREFETCH;
                localTicks += codeTicksAccessSeq16(armNextPC) + codeTicksAccess16(armNextPC) + 2;
                busPrefetchCount = 0;
            }
            handledInline = true;
        }

        // ========================================================================
        // FALLBACK: The original 1,024 instruction jump table
        // ========================================================================
        if (!handledInline) {
            clockTicks = 0;
            (*thumbInsnTable[opcode>>6])(opcode);
            localTicks = clockTicks; // Extract resulting global payload
        }

		if (localTicks < 0)
		  return 0;
		if (localTicks == 0)
		  localTicks = codeTicksAccessSeq16(oldArmNextPC) + 1; // oldArmNextPC is correct here for fallbacks lacking tick assignment

        // Correctly restores ticks for unhandled fallbacks lacking explicit overrides
        if (localTicks == 0)
        	localTicks = codeTicksAccessSeq16(oldArmNextPC) + 1;

        cpuTotalTicks += localTicks;

    } while (cpuTotalTicks < cpuNextEvent && !armState && !holdState && !SWITicks);

    return 1;
}
#endif

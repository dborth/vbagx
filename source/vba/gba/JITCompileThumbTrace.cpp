#include "JITCache.h"
#include "JITPPCEmitter.h"
#include "GBAinline.h"
#include "GBAcpu.h"
#include "JITProfiler.h"

#define MAX_WORDS 1024

// STATIC TIMING MACROS
// Prevents the JIT compiler from mutating the busPrefetchCount state during trace compilation.
#define STATIC_CODE_TICKS_SEQ16(addr) memoryWaitSeq[((addr) >> 24) & 15]
#define STATIC_CODE_TICKS_16(addr)    memoryWait[((addr) >> 24) & 15]
#define STATIC_DATA_TICKS_32(addr)    memoryWait32[((addr) >> 24) & 15]
#define STATIC_DATA_TICKS_16(addr)    memoryWait[((addr) >> 24) & 15]

static inline void FlushJITCache(void* addr, u32 size) {
    u32 start = (u32)addr & ~31;
    u32 end   = ((u32)addr + size + 31) & ~31;
    for (u32 i = start; i < end; i += 32) asm volatile("dcbst 0, %0" : : "r" (i) : "memory");
    asm volatile("sync" : : : "memory");
    for (u32 i = start; i < end; i += 32) asm volatile("icbi 0, %0" : : "r" (i) : "memory");
    asm volatile("sync \n isync" : : : "memory");
}

BasicBlock* JITCompileThumbTrace(u32 startPC, JITCache& cache) {
    u32* emitPtr = cache.allocateJITMemory(MAX_WORDS * sizeof(u32));
    u32* blockStart = emitPtr;

    u32 currentPC = startPC;
    u32 instrCount = 0;
    // O(1) Chunk Tracking State
	u32 chunkStartPC = startPC;
	u32 chunkInstrCount = 0;
	u32 chunkStaticCycles = 0;
    bool endBlock = false;

    auto EmitPrefetchSync = [&](u32*& ptr, u32 cInstrCount, u32 cStaticCost, u32 pc) {
		if (cInstrCount == 0) {
			if (cStaticCost > 0) *ptr++ = PPC_ADDI(PPC_R3, PPC_R3, cStaticCost);
			return;
		}

		u32 bank = (pc >> 24) & 15;
		u32 seqCost = memoryWaitSeq[bank];

		if (cStaticCost > 0) *ptr++ = PPC_ADDI(PPC_R3, PPC_R3, cStaticCost);

		// Only ROM banks have utilized prefetch buffers
		if (bank >= 0x08 && bank <= 0x0D) {
			// 1. Calculate available prefetch hits: C = 31 - cntlzw((R5 & 0xFF) + 1)
			*ptr++ = PPC_RLWINM(PPC_R11, PPC_R5, 0, 24, 31);
			*ptr++ = PPC_ADDI(PPC_R11, PPC_R11, 1);
			*ptr++ = PPC_CNTLZW(PPC_R11, PPC_R11);
			*ptr++ = PPC_LI(PPC_R10, 31);
			*ptr++ = PPC_SUBF(PPC_R11, PPC_R11, PPC_R10);

			// 2. Clamp hits to instructions executed: H = min(C, instrCount)
			*ptr++ = PPC_CMPWI(0, PPC_R11, cInstrCount);
			*ptr++ = PPC_BLE(8); // If C <= instrCount, skip the override clamp
			*ptr++ = PPC_LI(PPC_R11, cInstrCount);

			// 3. Subtract H * seqCost from R3
			if (seqCost == 1) {
				*ptr++ = PPC_SUBF(PPC_R3, PPC_R11, PPC_R3);
			} else if (seqCost == 2) {
				*ptr++ = PPC_RLWINM(PPC_R10, PPC_R11, 1, 0, 30); // R10 = H * 2
				*ptr++ = PPC_SUBF(PPC_R3, PPC_R10, PPC_R3);
			} else {
				*ptr++ = PPC_MULLI(PPC_R10, PPC_R11, seqCost);
				*ptr++ = PPC_SUBF(PPC_R3, PPC_R10, PPC_R3);
			}

			// 4. Consume hits from the prefetch buffer: R5 >>= H
			*ptr++ = PPC_RLWINM(PPC_R10, PPC_R5, 0, 0, 23); // Preserve upper bits
			*ptr++ = PPC_RLWINM(PPC_R5, PPC_R5, 0, 24, 31);
			*ptr++ = PPC_SRW(PPC_R5, PPC_R5, PPC_R11);
			*ptr++ = PPC_OR(PPC_R5, PPC_R5, PPC_R10);

			// 5. Hardware reset quirk: If H == 0 and R5 was > 0xFF, clear R5.
			*ptr++ = PPC_CMPWI(0, PPC_R11, 0);
			*ptr++ = PPC_BNE(16); // Skip if H != 0 (16 bytes = 4 instructions)
			*ptr++ = PPC_CMPWI(0, PPC_R10, 0); // Check upper bits
			*ptr++ = PPC_BEQ(8);  // Skip if R10 == 0
			*ptr++ = PPC_LI(PPC_R5, 0);
		}
	};

    auto EmitPrefetchDataWait = [&](u32*& ptr, u32 bankReg, u32 dataWaitStateReg) {
		// Natively emulates C++ recharge: busPrefetchCount = ((1 << waitState) - 1) | 0x100
		*ptr++ = PPC_CMPWI(0, bankReg, 8);
		*ptr++ = PPC_BLT(12);       // If bank < 8 (WRAM/IWRAM), branch to Recharge Path
		*ptr++ = PPC_LI(PPC_R5, 0); // Bank >= 8 (ROM) Data Read completely flushes prefetch!
		*ptr++ = PPC_B(28);         // Skip recharge logic (7 instructions * 4 = 28 bytes)

		// Recharge Path (Bank < 8)
		*ptr++ = PPC_CMPWI(0, PPC_R5, 0);
		*ptr++ = PPC_BNE(20);       // Skip recharge if buffer already active (R5 != 0)

		*ptr++ = PPC_LI(PPC_R5, 1);
		*ptr++ = PPC_SLW(PPC_R5, PPC_R5, dataWaitStateReg); // 1 << waitState (handles 0 natively)
		*ptr++ = PPC_ADDI(PPC_R5, PPC_R5, -1);              // (1 << waitState) - 1
		*ptr++ = PPC_ORI(PPC_R5, PPC_R5, 0x100);            // | 0x100
	};

    JIT_LOG_BLOCK_COMPILED(startPC);

    // R3 doubles as a LIVE runtime accumulator for data-access wait-state cycles
    // Every exit stub below ADDs chunkStaticCycles into R3 rather than overwriting it
    *emitPtr++ = PPC_LI(PPC_R3, 0);

    while (!endBlock && instrCount < 64) {
        // BUFFER OVERFLOW PROTECTION: Ensure we have enough words for the worst-case instruction + Epilogue
        if ((emitPtr - blockStart) > (MAX_WORDS - 64)) {
            endBlock = true;
            JIT_LOG_BAILOUT(currentPC, 0, BAILOUT_BUFFER_OVERFLOW);
            break;
        }

    	u16 opcode = CPUReadHalfWord(currentPC);

		// THUMB Formats 1, 2, 3, 4 - Native ALU (Shifts, Add, Sub, Mov, Cmp)
		if ((opcode & 0xC000) == 0x0000 || (opcode & 0xFC00) == 0x4000) {
			// Format 3: Move, Compare, Add, Subtract Immediate
			if ((opcode & 0xE000) == 0x2000) {
				u8 op = (opcode >> 11) & 0x03; // 0=MOV, 1=CMP, 2=ADD, 3=SUB
				u8 rd = (opcode >> 8) & 0x07;
				u8 imm = opcode & 0xFF;

				static const char* fmt[] = { "MOV", "CMP", "ADD", "SUB" };
				JIT_LOG_INSN_COMPILED(currentPC, opcode, "%s R%u, #0x%02X", fmt[op], rd, imm);

				if (op == 0) { // MOV
					*emitPtr++ = PPC_LI(MapGBARegister(rd), imm);
					*emitPtr++ = PPC_LI(PPC_REG_N, 0);
					*emitPtr++ = PPC_LI(PPC_REG_Z, (imm == 0) ? 1 : 0);
				} else {
					*emitPtr++ = PPC_LI(PPC_R12, imm);

					if (op == 1) { // CMP (Rd - Imm, result discarded)
						*emitPtr++ = PPC_SUBFCO(PPC_R11, PPC_R12, MapGBARegister(rd)); // R11 = Rd - Imm
					} else if (op == 2) { // ADD
						*emitPtr++ = PPC_ADDCO(MapGBARegister(rd), MapGBARegister(rd), PPC_R12);
					} else if (op == 3) { // SUB
						*emitPtr++ = PPC_SUBFCO(MapGBARegister(rd), PPC_R12, MapGBARegister(rd));
					}

					// Extract Hardware C and V Flags from XER natively
					*emitPtr++ = PPC_MFXER(PPC_R10);
					*emitPtr++ = PPC_RLWINM(PPC_REG_C, PPC_R10, 3, 31, 31);
					*emitPtr++ = PPC_RLWINM(PPC_REG_V, PPC_R10, 2, 31, 31);

					// Extract N and Z Flags
					u32 flagSrc = (op == 1) ? PPC_R11 : MapGBARegister(rd);
					*emitPtr++ = PPC_SRWI(PPC_REG_N, flagSrc, 31);
					*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, flagSrc);
					// STRICT Z-FLAG CLAMP: Rotate IBM Bit 26 (value 32) to Bit 31, and mask ONLY Bit 31.
					*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);
				}
				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			// Format 2: ADD / SUB (Register & Immediate)
			else if ((opcode & 0xF800) == 0x1800) {
				u8 rd = opcode & 0x07;
				u8 rs = (opcode >> 3) & 0x07;
				u8 type = (opcode >> 9) & 0x03; // 0=ADD reg, 1=SUB reg, 2=ADD imm, 3=SUB imm
				u8 rn_imm = (opcode >> 6) & 0x07;

				// Stage the right-hand operand natively into R12
				if (type == 0 || type == 1) *emitPtr++ = PPC_OR(PPC_R12, MapGBARegister(rn_imm), MapGBARegister(rn_imm));
				else                        *emitPtr++ = PPC_LI(PPC_R12, rn_imm);

				// Execute Math utilizing Broadway's Fixed-Point Exception Register (XER)
				if (type == 0 || type == 2) *emitPtr++ = PPC_ADDCO(MapGBARegister(rd), MapGBARegister(rs), PPC_R12); // ADD
				else                        *emitPtr++ = PPC_SUBFCO(MapGBARegister(rd), PPC_R12, MapGBARegister(rs)); // SUB (Rs - R12)

				// Extract Hardware C and V Flags from XER (Branchless)
				*emitPtr++ = PPC_MFXER(PPC_R11);
				*emitPtr++ = PPC_RLWINM(PPC_REG_C, PPC_R11, 3, 31, 31); // IBM Bit 2 (CA) -> Bit 31
				*emitPtr++ = PPC_RLWINM(PPC_REG_V, PPC_R11, 2, 31, 31); // IBM Bit 1 (OV) -> Bit 31

				// Extract N and Z Flags
				*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31);
				*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));
				// STRICT Z-FLAG CLAMP: Rotate IBM Bit 26 (value 32) to Bit 31, and mask ONLY Bit 31.
				*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);

				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			// Format 1: LSL / LSR / ASR
			else if ((opcode & 0x1800) != 0x1800 && (opcode & 0xE000) == 0x0000) {
				u8 rd = opcode & 0x07;
				u8 rs = (opcode >> 3) & 0x07;
				u8 offset = (opcode >> 6) & 0x1F;
				u8 op = (opcode >> 11) & 0x03; // 0=LSL, 1=LSR, 2=ASR

				if (op == 0) { // LSL
					if (offset == 0) {
						*emitPtr++ = PPC_OR(MapGBARegister(rd), MapGBARegister(rs), MapGBARegister(rs)); // MOV
					} else {
						// Correct IBM Bit-Math: Rotate Left by (offset)
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), offset, 31, 31);
						*emitPtr++ = PPC_RLWINM(MapGBARegister(rd), MapGBARegister(rs), offset, 0, 31 - offset);
					}
				}
				else if (op == 1) { // LSR
					if (offset == 0) {
						// LSR #32: ARM bit 31 (IBM bit 0) goes to carry. Rotate Left by 1.
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), 1, 31, 31);
						*emitPtr++ = PPC_LI(MapGBARegister(rd), 0);
					} else {
						// Correct IBM Bit-Math: Rotate Left by (33 - offset) & 31
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), (33 - offset) & 31, 31, 31);
						*emitPtr++ = PPC_SRWI(MapGBARegister(rd), MapGBARegister(rs), offset);
					}
				}
				else if (op == 2) { // ASR
					if (offset == 0) {
						// ASR #32: ARM bit 31 (IBM bit 0) goes to carry. Rotate Left by 1.
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), 1, 31, 31);
						*emitPtr++ = PPC_SRAWI(MapGBARegister(rd), MapGBARegister(rs), 31);
					} else {
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), (33 - offset) & 31, 31, 31);
						*emitPtr++ = PPC_SRAWI(MapGBARegister(rd), MapGBARegister(rs), offset);
					}
				}

				// Extract N and Z Flags
				*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31);
				*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));
				// STRICT Z-FLAG CLAMP: Rotate IBM Bit 26 (value 32) to Bit 31, and mask ONLY Bit 31.
				*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);
				
				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			// Format 4: ALU Operations
			else if ((opcode & 0xFC00) == 0x4000) {
				u8 op = (opcode >> 6) & 0x0F;
				u8 rs = (opcode >> 3) & 0x07;
				u8 rd = opcode & 0x07;

				if (op == 10) { // CMP (Compare)
					*emitPtr++ = PPC_SUBFCO(PPC_R12, MapGBARegister(rs), MapGBARegister(rd)); // R12 = Rd - Rs

					*emitPtr++ = PPC_MFXER(PPC_R11);
					*emitPtr++ = PPC_RLWINM(PPC_REG_C, PPC_R11, 3, 31, 31);
					*emitPtr++ = PPC_RLWINM(PPC_REG_V, PPC_R11, 2, 31, 31);

					*emitPtr++ = PPC_SRWI(PPC_REG_N, PPC_R12, 31);
					*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, PPC_R12);
					*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 0 || op == 1 || op == 12 || op == 14) { // AND, EOR, ORR, BIC
					if (op == 0)  *emitPtr++ = PPC_AND(MapGBARegister(rd), MapGBARegister(rd), MapGBARegister(rs));
					if (op == 1)  *emitPtr++ = PPC_XOR(MapGBARegister(rd), MapGBARegister(rd), MapGBARegister(rs));
					if (op == 12) *emitPtr++ = PPC_OR(MapGBARegister(rd), MapGBARegister(rd), MapGBARegister(rs));
					if (op == 14) *emitPtr++ = PPC_ANDC(MapGBARegister(rd), MapGBARegister(rd), MapGBARegister(rs)); // BIC

					// Extract N and Z Flags
					*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31);
					*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));
					*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				} else if (op == 9) { // NEG (Rd = 0 - Rs)
					*emitPtr++ = PPC_LI(PPC_R12, 0); // Load 0 into scratch
					*emitPtr++ = PPC_SUBFCO(MapGBARegister(rd), MapGBARegister(rs), PPC_R12); // Rd = R12 (0) - Rs

					// Extract Hardware C and V Flags natively from XER
					*emitPtr++ = PPC_MFXER(PPC_R11);
					*emitPtr++ = PPC_RLWINM(PPC_REG_C, PPC_R11, 3, 31, 31);
					*emitPtr++ = PPC_RLWINM(PPC_REG_V, PPC_R11, 2, 31, 31);

					// Extract N and Z Flags
					*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31);
					*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));
					*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 15) { // MVN (Bitwise NOT: Rd = ~Rs)
					*emitPtr++ = PPC_NOR(MapGBARegister(rd), MapGBARegister(rs), MapGBARegister(rs));

					// MVN only updates N and Z
					*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31);
					*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));
					*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 8) { // TST (AND flags only, discard result)
					*emitPtr++ = PPC_AND(PPC_R12, MapGBARegister(rd), MapGBARegister(rs)); // R12 = Rd & Rs

					// TST only updates N and Z
					*emitPtr++ = PPC_SRWI(PPC_REG_N, PPC_R12, 31);
					*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, PPC_R12);
					*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);

					chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
				}
				else if (op == 13) { // MUL (Rd = Rd * Rs)
					// thumb43_1's real cost depends on the ORIGINAL Rd's magnitude
					// (ARM7TDMI multiply early-termination): rm = |original Rd|-ish
					// (sign-complemented if negative), active_bits = 31-clz(rm|1),
					// cost = 2 + (active_bits>>3) [0..3] + codeTicksAccess16(armNextPC).
					// Save the original Rd before MULLW overwrites it.
					*emitPtr++ = PPC_OR(PPC_R11, MapGBARegister(rd), MapGBARegister(rd));

					*emitPtr++ = PPC_MULLW(MapGBARegister(rd), MapGBARegister(rd), MapGBARegister(rs));

					// rm = (original Rd < 0) ? ~original Rd : original Rd - branchless
					// via arithmetic-shift sign mask (0 or -1), same trick as thumb43_1.
					*emitPtr++ = PPC_SRAWI(PPC_R12, PPC_R11, 31);
					*emitPtr++ = PPC_XOR(PPC_R11, PPC_R11, PPC_R12);
					*emitPtr++ = PPC_ORI(PPC_R11, PPC_R11, 1); // avoid clz(0)

					// active_bits = 31 - cntlzw(rm); (>>3) maps 0-31 to 0..3
					*emitPtr++ = PPC_CNTLZW(PPC_R12, PPC_R11);
					*emitPtr++ = PPC_LI(PPC_R10, 31);
					*emitPtr++ = PPC_SUBF(PPC_R12, PPC_R12, PPC_R10);
					*emitPtr++ = PPC_SRWI(PPC_R12, PPC_R12, 3);
					*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R12);

					// MUL only updates N and Z in Thumb
					*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31);
					*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));
					*emitPtr++ = PPC_RLWINM(PPC_REG_Z, PPC_REG_Z, 27, 31, 31);

					// Non-sequential code-fetch table - thumb43_1 uses codeTicksAccess16
					// (non-seq) even though MUL isn't a guest memory access; that's just
					// what the real multiplier timing quirk calls. Flat "+2" (was "+1",
					// missing thumb43_1's separate "clockTicks = 1" base plus its final "+ 1").
					chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + 2;
				} else {
					endBlock = true;
					JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNSUPPORTED_ALU);
					break;
				}
			}
		}
		// THUMB Format 5 - High Register Operations (MOV / ADD)
		else if ((opcode & 0xFC00) == 0x4400 && ((opcode >> 8) & 0x03) != 3) {
			u8 op = (opcode >> 8) & 0x03; // 0=ADD, 1=CMP, 2=MOV
			u8 h1 = (opcode >> 7) & 0x01;
			u8 h2 = (opcode >> 6) & 0x01;
			u8 rs = (opcode >> 3) & 0x07;
			u8 rd = opcode & 0x07;

			u8 actualRs = rs | (h2 << 3);
			u8 actualRd = rd | (h1 << 3);

			// Modifying the Program Counter directly triggers a branch pipeline flush.
			// We bail these out to C++ to handle the complex timing sync.
			if (actualRd == 15) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_CONDITIONAL_BRANCH);
				break;
			}

			if (op == 2) { // MOV
				if (actualRs == 15) {
					*emitPtr++ = PPC_LIS(MapGBARegister(actualRd), (currentPC + 4) >> 16);
					*emitPtr++ = PPC_ORI(MapGBARegister(actualRd), MapGBARegister(actualRd), (currentPC + 4) & 0xFFFF);
				} else {
					*emitPtr++ = PPC_OR(MapGBARegister(actualRd), MapGBARegister(actualRs), MapGBARegister(actualRs));
				}
				
				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			}
			else if (op == 0) { // ADD
				if (actualRs == 15) {
					*emitPtr++ = PPC_LIS(PPC_R12, (currentPC + 4) >> 16);
					*emitPtr++ = PPC_ORI(PPC_R12, PPC_R12, (currentPC + 4) & 0xFFFF);
					*emitPtr++ = PPC_ADD(MapGBARegister(actualRd), MapGBARegister(actualRd), PPC_R12);
				} else {
					*emitPtr++ = PPC_ADD(MapGBARegister(actualRd), MapGBARegister(actualRd), MapGBARegister(actualRs));
				}
				
				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC) + 1;
			} else {
				// High-Register CMP (op == 1) requires flag updates. Bail for now.
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_HIGH_REGISTER_CMP);
				break;
			}
		}
		// THUMB Format 5 - Branch Exchange (BX Rs)
		else if ((opcode & 0xFF00) == 0x4700) {
			u8 rs = (opcode >> 3) & 0x0F;

			// Protect against dynamic reads of R15 causing a stale PC desync
			if (rs == 15) {
				*emitPtr++ = PPC_LIS(PPC_R12, (currentPC + 4) >> 16);
				*emitPtr++ = PPC_ORI(PPC_R12, PPC_R12, (currentPC + 4) & 0xFFFF);
			} else {
				*emitPtr++ = PPC_OR(PPC_R12, MapGBARegister(rs), MapGBARegister(rs));
			}

			// Extract Bit 0 to check if we are switching to ARM mode
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R12, 0, 31, 31);
			*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);

			u32* branchArmSwitch = emitPtr;
			*emitPtr++ = PPC_BEQ(0); // If Bit 0 == 0 (ARM), jump to C++ bailout

			// TRUE PATH: Stay in THUMB, exit block dynamically
			// PIPELINE SYNC: Dynamic branch forces a pipeline flush (+3 cycles)
			u32 takenPenalty = STATIC_CODE_TICKS_SEQ16(currentPC) + 3;
			EmitPrefetchSync(emitPtr, chunkInstrCount + 1, chunkStaticCycles + takenPenalty, chunkStartPC);
			*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer
			*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R12, 0, 0, 30); // R4 = TargetPC & ~1
			*emitPtr++ = PPC_BLR();

			// FALSE PATH: ARM Switch Bailout
			u32* bailoutTarget = emitPtr;
			*emitPtr++ = PPC_ADDI(PPC_R3, PPC_R3, chunkStaticCycles);
			*emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
			*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
			*emitPtr++ = PPC_BLR();

			*branchArmSwitch = PPC_BEQ((u32)((bailoutTarget - branchArmSwitch) * 4));

			endBlock = true;
			JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_ARM_SWITCH);
			break;
		}
		// THUMB Format 6 - PC-Relative Load (LDR Rd, [PC, #Imm])
		else if ((opcode & 0xF800) == 0x4800) {
			u8 rd = (opcode >> 8) & 0x07;
			u8 imm = opcode & 0xFF;
			u32 targetAddr = ((currentPC + 4) & ~3) + (imm << 2);

			JIT_LOG_INSN_COMPILED(currentPC, opcode, "LDR R%u, [PC, #0x%X] ; (0x%08X)", rd, imm, targetAddr);

			u8 bank = targetAddr >> 24;
			// ONLY statically bake BIOS (0x00) and ROM (0x08-0x0D).
			// Bank 0x0E+ contains SRAM/Registers which mutate!
			if (bank == 0x00 || (bank >= 0x08 && bank <= 0x0D)) {
				u32 loadedValue = CPUReadMemory(targetAddr);

				if ((loadedValue & ~0x7FFF) == 0) {
					*emitPtr++ = PPC_LI(MapGBARegister(rd), loadedValue);
				} else if ((loadedValue & ~0xFFFF) == 0) {
					*emitPtr++ = PPC_LI(MapGBARegister(rd), 0);
					*emitPtr++ = PPC_ORI(MapGBARegister(rd), MapGBARegister(rd), loadedValue);
				} else if ((loadedValue & 0xFFFF) == 0) {
					*emitPtr++ = PPC_LIS(MapGBARegister(rd), loadedValue >> 16);
				} else {
					*emitPtr++ = PPC_LIS(MapGBARegister(rd), loadedValue >> 16);
					*emitPtr++ = PPC_ORI(MapGBARegister(rd), MapGBARegister(rd), loadedValue & 0xFFFF);
				}
				// Format 6 is always a 32-bit load, and its target is a compile-time
				// constant, so (unlike Format 9/10/11) the real data-access wait-state
				// cost folds directly into staticCycles here. Matches thumb48's
				// `3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC)`:
				// non-sequential code table (a memory access breaks the sequential
				// prefetch stream), +3 (load constant, not +2 which was the store one).
				chunkStaticCycles += STATIC_DATA_TICKS_32(targetAddr) + STATIC_CODE_TICKS_16(currentPC) + 3;

				// Format 6 reads from ROM/BIOS, so it breaks the prefetch stream.
				EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0);
				chunkInstrCount = 0;
				chunkStaticCycles = 0;
				chunkStartPC = currentPC + 2;
			} else {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNSUPPORTED_MEM_BANK);
				break;
			}
		}
		// THUMB Formats 9, 10, 11 - Unified Memory Loads AND Stores
		else if (((opcode & 0xE000) == 0x6000) || ((opcode & 0xF000) == 0x5000) || ((opcode & 0xF000) == 0x8000)) {

		    bool isMemLoad = false;
		    bool isMemStore = false;
		    u8 rd, rb, ro, imm;
		    u32 accessType = 0; // 4=Word, 2=Halfword, 1=Byte

			// Format 9: LDR/STR Rd, [Rb, #Imm] (Word Access)
		    if ((opcode & 0xF000) == 0x6000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; imm = (opcode >> 6) & 0x1F;
		        *emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm << 2);
		        accessType = 4;
				if (opcode & 0x0800) isMemLoad = true; else isMemStore = true;
		    }
		    // Format 9: LDRB/STRB Rd, [Rb, #Imm] (Byte Access)
		    else if ((opcode & 0xF000) == 0x7000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; imm = (opcode >> 6) & 0x1F;
		        *emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm);
		        accessType = 1;
		        if (opcode & 0x0800) isMemLoad = true; else isMemStore = true;
		    }
		    // Format 11: LDRH/STRH Rd, [Rb, #Imm] (Halfword Access)
		    else if ((opcode & 0xF000) == 0x8000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; imm = (opcode >> 6) & 0x1F;
		        *emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm << 1);
		        accessType = 2;
				if (opcode & 0x0800) isMemLoad = true; else isMemStore = true;
		    }
		    // Format 10: Register Offset Loads & Stores (LDR, LDRH, LDRB, STR, STRH, STRB)
		    else if ((opcode & 0xF000) == 0x5000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; ro = (opcode >> 6) & 0x07;
		        u16 subOp = opcode & 0x0E00;
		        if (subOp <= 0x0C00 && subOp != 0x0600) {
		            *emitPtr++ = PPC_ADD(PPC_R12, MapGBARegister(rb), MapGBARegister(ro));
		            if (subOp == 0x0800) { isMemLoad = true; accessType = 4; }
		            else if (subOp == 0x0A00) { isMemLoad = true; accessType = 2; }
		            else if (subOp == 0x0C00) { isMemLoad = true; accessType = 1; }
		            else if (subOp == 0x0000) { isMemStore = true; accessType = 4; }
		            else if (subOp == 0x0200) { isMemStore = true; accessType = 2; }
		            else if (subOp == 0x0400) { isMemStore = true; accessType = 1; }
		        }
		    }

		    if (isMemLoad || isMemStore) {
		        if (instrCount == 0) {
		            endBlock = true;
		            JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_TMB9_10_11_INSTR_COUNT_ZERO);
		            break;
		        }

				EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
				chunkInstrCount = 0;
				chunkStaticCycles = 0;
				chunkStartPC = currentPC + 2;

		        // 1. Extract Memory Bank (R12 >> 24)
		        *emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);
		        // Stash the bank in R4 (instruction-local scratch only - nothing here
				// needs it to survive to the next instruction) since R11 gets
				// overwritten by the page/mask lookup below, and we need the bank
				// again afterward for the data-access cycle-cost lookup.
		        *emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R11, 0, 28, 31); // R4 = R11 & 15

		        u32* branchGuard1 = nullptr;
		        u32* branchGuard2 = nullptr;
		        u32* branchGuard3 = nullptr;

		        if (isMemStore) {
		            // STORE STRICT GUARD: Only Banks 2 & 3 (WRAM) allowed
		            *emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
		            *emitPtr++ = PPC_BEQ(12);
		            *emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
		            branchGuard1 = emitPtr;
		            *emitPtr++ = PPC_BNE(0);
		        } else {
		            // LOAD GUARD: Allow WRAM and ROM. Block BIOS (0), MMIO (4), and EEPROM/SRAM (>= 0x0D)
		            *emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
		            branchGuard3 = emitPtr;
		            *emitPtr++ = PPC_BEQ(0);
		            *emitPtr++ = PPC_CMPWI(0, PPC_R11, 4);
		            branchGuard1 = emitPtr;
		            *emitPtr++ = PPC_BEQ(0);
		            *emitPtr++ = PPC_CMPWI(0, PPC_R11, 13);
		            branchGuard2 = emitPtr;
		            *emitPtr++ = PPC_BGE(0);
		        }

		        // 2. Load Page Pointer and Memory Mask
		        *emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29); // R11 = Bank * 4
		        *emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
		        *emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

				// 3. UNIVERSAL NULL POINTER GUARD (Prevents hardware DSI crashes)
		        *emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
		        u32* branchNullToBailout = emitPtr;
		        *emitPtr++ = PPC_BEQ(0);

		        // 4. Apply Memory Mask
		        *emitPtr++ = PPC_AND(PPC_R12, PPC_R12, PPC_R11);

				// 4.5 RUNTIME DATA-ACCESS CYCLE LOOKUP (safe path only - every guard
				// above has already passed by this point, so R3 is correctly left
				// untouched on every bailout path). Mirrors dataTicksAccess32/16:
				// word accesses use memoryWait32[], byte/halfword share memoryWait[].
				// The bank isn't known until runtime here (unlike Format 6's
				// PC-relative case), so this has to be an emitted table lookup
				// rather than a compile-time constant.
		        {
		            u8* dataTicksTable = (accessType == 4) ? memoryWait32 : memoryWait;
		            *emitPtr++ = PPC_LIS(PPC_R11, ((u32)dataTicksTable) >> 16);
		            *emitPtr++ = PPC_ORI(PPC_R11, PPC_R11, ((u32)dataTicksTable) & 0xFFFF);
		            *emitPtr++ = PPC_LBZX(PPC_R11, PPC_R4, PPC_R11); // EA = R4 + R11
		            *emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R11);

		            EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R11);
		        }

		        // 5. Execute Memory Load or Store Instruction
		        if (isMemLoad) {
		            if (accessType == 4) *emitPtr++ = PPC_LWBRX(MapGBARegister(rd), PPC_R10, PPC_R12);
		            else if (accessType == 2) *emitPtr++ = PPC_LHBRX(MapGBARegister(rd), PPC_R10, PPC_R12);
		            else *emitPtr++ = PPC_LBZX(MapGBARegister(rd), PPC_R10, PPC_R12);
		        } else {
		            if (accessType == 4) *emitPtr++ = PPC_STWBRX(MapGBARegister(rd), PPC_R10, PPC_R12);
		            else if (accessType == 2) *emitPtr++ = PPC_STHBRX(MapGBARegister(rd), PPC_R10, PPC_R12);
		            else *emitPtr++ = PPC_STBZX(MapGBARegister(rd), PPC_R10, PPC_R12);
		        }

		        // 6. Jump Over Bailout Stub
		        u32* branchSafe = emitPtr;
		        *emitPtr++ = PPC_B(0);

		        // 7. Bailout Stub Generation
		        u32* bailoutTarget = emitPtr;
		        *emitPtr++ = PPC_ADDI(PPC_R3, PPC_R3, chunkStaticCycles);
		        *emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
		        *emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
		        *emitPtr++ = PPC_BLR();

		        // 8. Patch Branch Offsets
				if (branchGuard1) *branchGuard1 = (isMemStore) ? PPC_BNE((u32)((bailoutTarget - branchGuard1) * 4)) : PPC_BEQ((u32)((bailoutTarget - branchGuard1) * 4));
		        if (branchGuard2) *branchGuard2 = PPC_BGE((u32)((bailoutTarget - branchGuard2) * 4));
		        if (branchGuard3) *branchGuard3 = PPC_BEQ((u32)((bailoutTarget - branchGuard3) * 4));

		        *branchNullToBailout = PPC_BEQ((u32)((bailoutTarget - branchNullToBailout) * 4));

		        u32* safeTarget = emitPtr;
		        *branchSafe = PPC_B((u32)((safeTarget - branchSafe) * 4));
				
				// Non-sequential table for the code-fetch component: a data-bus access
				// disrupts the prefetch pipeline, so the interpreter's real formula
				// (thumb68 etc: `dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2/3`)
				// always pays the non-sequential cost for the instruction after a memory op.
		        chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + ((isMemStore) ? 2 : 3);
		    } else {
		        endBlock = true;
		        JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNKNOWN_MEM_OP);
		        break;
		    }
		}
		// THUMB Format 11: SP-relative Load/Store (LDR/STR Rd, [SP, #imm])
		else if ((opcode & 0xF000) == 0x9000) {
			if (instrCount == 0) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_TMB9_10_11_INSTR_COUNT_ZERO);
				break;
			}
			
			// SP-relative loads/stores also break the prefetch stream!
			EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
			chunkInstrCount = 0;
			chunkStaticCycles = 0;
			chunkStartPC = currentPC + 2;

			u8 isLoad = (opcode >> 11) & 0x01;
			u8 rd     = (opcode >> 8) & 0x07;
			u32 offset = (opcode & 0xFF) << 2;

			// 1. Calculate Target Address in PPC_R12 (SP + offset)
			if (offset == 0) {
				*emitPtr++ = PPC_OR(PPC_R12, MapGBARegister(13), MapGBARegister(13));
			} else {
				*emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(13), offset);
			}

			// 2. Bank Guard Check: Check if EA is in EWRAM (0x02) or IWRAM (0x03)
			// (EA >> 25) == 1 is true IF AND ONLY IF bank is 2 or 3.
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R12, 7, 25, 31); // R11 = EA >> 25
			*emitPtr++ = PPC_CMPWI(0, PPC_R11, 1);

			u32* branchToBailout = emitPtr++; // Placeholder for PPC_BNE

			// 3. Host Address Translation
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R12, 8, 24, 31); // R11 = bank (EA >> 24)
			// Stash the bank in R4 (unused elsewhere in this handler) before R11
			// gets turned into a table index below - needed for the data-tick
			// lookup once the guard has passed.
			*emitPtr++ = PPC_OR(PPC_R4, PPC_R11, PPC_R11);
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29);  // R11 = bank * 4
			*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11); // R10 = readPages[bank]
			*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11); // R11 = readMasks[bank]
			*emitPtr++ = PPC_AND(PPC_R12, PPC_R12, PPC_R11);       // R12 = EA & mask

			// 3.5 RUNTIME DATA-ACCESS CYCLE LOOKUP (safe path only - the guard
			// above has already passed by this point). Always a 32-bit access,
			// so always memoryWait32[] - matches thumb90/thumb98's
			// dataTicksAccess32(address). The bank is fixed to 2 or 3 by the
			// guard, but not known at compile time (SP is a runtime value).
			*emitPtr++ = PPC_LIS(PPC_R11, ((u32)memoryWait32) >> 16);
			*emitPtr++ = PPC_ORI(PPC_R11, PPC_R11, ((u32)memoryWait32) & 0xFFFF);
			*emitPtr++ = PPC_LBZX(PPC_R11, PPC_R4, PPC_R11); // Safe: rA=R4
			*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R11);
			EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R11); // Recharge prefetch buffer

			// 4. Endian-Correct Load or Store
			if (isLoad) {
				*emitPtr++ = PPC_LWBRX(MapGBARegister(rd), PPC_R10, PPC_R12);
			} else {
				*emitPtr++ = PPC_STWBRX(MapGBARegister(rd), PPC_R10, PPC_R12);
			}

			// Jump past the bailout stub on normal execution
			u32* branchToCont = emitPtr++;

			// 5. Bailout Stub (triggered if SP target is outside EWRAM/IWRAM)
			*branchToBailout = PPC_BNE((u32)(emitPtr - branchToBailout) * 4);
			*emitPtr++ = PPC_ADDI(PPC_R3, PPC_R3, chunkStaticCycles);
			*emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
			*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
			*emitPtr++ = PPC_BLR();

			// Patch jump to continuation
			*branchToCont = PPC_B((u32)(emitPtr - branchToCont) * 4);
			
			// thumb90 (STR): `dataTicksAccess32(address) + codeTicksAccess16(armNextPC) + 2`
			// thumb98 (LDR): `3 + dataTicksAccess32(address) + codeTicksAccess16(armNextPC)`
			// Non-sequential code-fetch table (a memory access breaks the sequential
			// prefetch stream), and the flat constant depends on load vs store -
			// this previously used +2 for both, which under-counts every LDR by 1.
			chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + (isLoad ? 3 : 2);
		}
    	// THUMB Format 14 - PUSH / POP
		else if ((opcode & 0xF600) == 0xB400) {
			bool isPop = (opcode & 0x0800) != 0;
			bool Rbit = (opcode & 0x0100) != 0;
			u8 regList = opcode & 0xFF;

			if (instrCount == 0) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_THB14_INSTR_COUNT_ZERO);
				break;
			}

			int numRegs = 0;
			for (int i = 0; i < 8; i++) if (regList & (1 << i)) numRegs++;
			if (Rbit) numRegs++;

			if (numRegs == 0) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_PUSH_POP_REGS);
				break;
			}

			EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
			chunkInstrCount = 0;
			chunkStaticCycles = 0;
			chunkStartPC = currentPC + 2;

			// Stage SP natively into R12
			if (!isPop) {
				// PUSH: Decrement SP first. PPC_ADDI handles negative offsets.
				*emitPtr++ = PPC_ADDI(MapGBARegister(13), MapGBARegister(13), -numRegs * 4);
				*emitPtr++ = PPC_OR(PPC_R12, MapGBARegister(13), MapGBARegister(13));
			} else {
				// POP: Use SP as base address directly.
				*emitPtr++ = PPC_OR(PPC_R12, MapGBARegister(13), MapGBARegister(13));
			}

			// 1. Extract Memory Bank (R12 >> 24)
			*emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);
			// Stash the bank in R4 (free until the per-register loop below starts
			// using it) since R11 becomes the mask and R5 becomes the address
			// tracker before we get a chance to look up the data-access cost.
			*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R11, 0, 28, 31); // R4 = R11 & 15
			bool pushPopAccumulateDataTicks = !(isPop && !Rbit);

			u32* branchGuard1 = nullptr;
			u32* branchGuard2 = nullptr;
			u32* branchGuard3 = nullptr;

			if (!isPop) {
				// STORE STRICT GUARD: Only Banks 2 & 3
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
				*emitPtr++ = PPC_BEQ(12);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
				branchGuard1 = emitPtr;
				*emitPtr++ = PPC_BNE(0);
			} else {
				// LOAD GUARD: Allow WRAM and ROM. Block BIOS (0), MMIO (4), and EEPROM/SRAM (>= 0x0D)
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
				branchGuard3 = emitPtr;
				*emitPtr++ = PPC_BEQ(0);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 4);
				branchGuard1 = emitPtr;
				*emitPtr++ = PPC_BEQ(0);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 13); // EEPROM BLOCK
				branchGuard2 = emitPtr;
				*emitPtr++ = PPC_BGE(0);
			}

			// 2. Load Page Pointer and Mask
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29); // R11 = Bank * 4
			*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
			*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

			// 3. Null Pointer Guard
			*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
			u32* branchNullToBailout = emitPtr;
			*emitPtr++ = PPC_BEQ(0);

			// 3.5 RUNTIME DATA-ACCESS CYCLE LOOKUP (safe path only).
			// PUSH_REG/POP_REG charge dataTicksAccess32 (non-seq) for the first
			// register and dataTicksAccessSeq32 (seq) for every register after
			// that, all in the same bank (SP never crosses a bank mid-instruction).
			// numRegs is compile-time-known, so the repeat count is unrolled here
			// (avoids mullw per Broadway convention) - only the two table values
			// themselves need a runtime lookup. Skipped entirely for POP{Rlist}
			// (see pushPopAccumulateDataTicks above).
			if (pushPopAccumulateDataTicks) {
				*emitPtr++ = PPC_LIS(PPC_R0, ((u32)memoryWait32) >> 16);
				*emitPtr++ = PPC_ORI(PPC_R0, PPC_R0, ((u32)memoryWait32) & 0xFFFF);
				*emitPtr++ = PPC_LBZX(PPC_R0, PPC_R4, PPC_R0); // EA = R4 (Bank) + R0 (Table)
				*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R0);
				EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R0); // Recharge during first access

				if (numRegs > 1) {
					*emitPtr++ = PPC_LIS(PPC_R0, ((u32)memoryWaitSeq32) >> 16);
					*emitPtr++ = PPC_ORI(PPC_R0, PPC_R0, ((u32)memoryWaitSeq32) & 0xFFFF);
					*emitPtr++ = PPC_LBZX(PPC_R0, PPC_R4, PPC_R0); // EA = R4 + R0
					for (int k = 0; k < numRegs - 1; k++) {
						*emitPtr++ = PPC_ADD(PPC_R3, PPC_R3, PPC_R0);
						EmitPrefetchDataWait(emitPtr, PPC_R4, PPC_R0); // Recharge during seq accesses
					}
				}
			}

			// 4. Memory Operations Loop
			for (int i = 0; i < 8; i++) {
				if (regList & (1 << i)) {
					*emitPtr++ = PPC_AND(PPC_R4, PPC_R12, PPC_R11); // Apply Mask
					if (isPop) *emitPtr++ = PPC_LWBRX(MapGBARegister(i), PPC_R10, PPC_R4);
					else       *emitPtr++ = PPC_STWBRX(MapGBARegister(i), PPC_R10, PPC_R4);
					*emitPtr++ = PPC_ADDI(PPC_R12, PPC_R12, 4); // Advance 4 bytes
				}
			}
			if (Rbit) {
				*emitPtr++ = PPC_AND(PPC_R4, PPC_R12, PPC_R11);
				if (isPop) *emitPtr++ = PPC_LWBRX(PPC_R12, PPC_R10, PPC_R4); // POP PC into R12 scratch
				else       *emitPtr++ = PPC_STWBRX(MapGBARegister(14), PPC_R10, PPC_R4); // PUSH LR (GBA R14)
			}

			// 5. Update SP (POP only)
			if (isPop) {
				*emitPtr++ = PPC_ADDI(MapGBARegister(13), MapGBARegister(13), numRegs * 4);
			}

			// 6. Branch out or Skip Bailout
			u32* branchSafe = nullptr;
			if (isPop && Rbit) {
				// POP PC: We must exit the block dynamically
				// PIPELINE SYNC: Dynamic branch forces a pipeline flush (+3 cycles)
				*emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R12, 0, 0, 30); // R4 = TargetPC & ~1
				// non-sequential table, doubled (extra pipeline refill for the PC-changing pop)
				u32 takenPenalty = chunkStaticCycles + numRegs + STATIC_CODE_TICKS_16(currentPC) * 2 + 3;
				EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles + takenPenalty, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer
				*emitPtr++ = PPC_BLR();
			} else {
				branchSafe = emitPtr;
				*emitPtr++ = PPC_B(0);
			}

			// 7. Bailout Stub Generation
			u32* bailoutTarget = emitPtr;

			// If PUSH fails a guard, we must revert the SP decrement before bailing out!
			if (!isPop) *emitPtr++ = PPC_ADDI(MapGBARegister(13), MapGBARegister(13), numRegs * 4);

			*emitPtr++ = PPC_ADDI(PPC_R3, PPC_R3, chunkStaticCycles);
			*emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
			*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
			*emitPtr++ = PPC_BLR();

			// 8. Patch Branch Offsets
			if (branchGuard1) *branchGuard1 = (!isPop) ? PPC_BNE((u32)((bailoutTarget - branchGuard1) * 4)) : PPC_BEQ((u32)((bailoutTarget - branchGuard1) * 4));
			if (branchGuard2) *branchGuard2 = PPC_BGE((u32)((bailoutTarget - branchGuard2) * 4));
			if (branchGuard3) *branchGuard3 = PPC_BEQ((u32)((bailoutTarget - branchGuard3) * 4));
			*branchNullToBailout = PPC_BEQ((u32)((bailoutTarget - branchNullToBailout) * 4));

			if (branchSafe) {
				u32* safeTarget = emitPtr;
				*branchSafe = PPC_B((u32)((safeTarget - branchSafe) * 4));
			}

			if (isPop && !Rbit) {
				// POP {Rlist}: thumbBC ends with `clockTicks = 2 + codeTicksAccess16(...)`
				// - a plain assignment, not +=, which discards every +1-per-register
				// AND every per-register data-tick that POP_REG built up. Verified
				// directly against GBA-thumb.cpp. Faithfully reproduce that discard
				// (numRegs deliberately excluded here) rather than "fixing" it into
				// something more hardware-accurate but interpreter-divergent.
				chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + 2;
			} else if (!(isPop && Rbit)) {
				// PUSH {Rlist} / PUSH {Rlist, LR}: thumbB4/B5 use += throughout, so
				// numRegs' +1s and their data-ticks (already accumulated into R3
				// above) both survive, plus each one's own flat "+1".
				chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + numRegs + 1;
			}
			// (POP {Rlist, PC} already emitted its own exit above and always ends
			// the block there, so it never reaches this trailing accumulation.)
		}
    	// THUMB Format 15 - Multiple Load/Store (LDMIA / STMIA)
		else if ((opcode & 0xF000) == 0xC000) {
			bool isLoad = (opcode & 0x0800) != 0;
			u8 rb = (opcode >> 8) & 0x07;
			u8 regList = opcode & 0xFF;

			if (instrCount == 0) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_THB15_INSTR_COUNT_ZERO);
				break;
			}

			int numRegs = 0;
			for (int i = 0; i < 8; i++) if (regList & (1 << i)) numRegs++;

			if (numRegs == 0) {
				endBlock = true;
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_LDMIA_STMIA_REGS);
				break;
			}

			EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
			chunkInstrCount = 0;
			chunkStaticCycles = 0;
			chunkStartPC = currentPC + 2;

			// Stage Base Address natively into R12
			*emitPtr++ = PPC_OR(PPC_R12, MapGBARegister(rb), MapGBARegister(rb));

			// 1. Extract Memory Bank (R12 >> 24)
			*emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);

			u32* branchGuard1 = nullptr;
			u32* branchGuard2 = nullptr;
			u32* branchGuard3 = nullptr;

			if (!isLoad) {
				// STMIA GUARD: Only Banks 2 & 3
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
				*emitPtr++ = PPC_BEQ(12);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
				branchGuard1 = emitPtr;
				*emitPtr++ = PPC_BNE(0);
			} else {
				// LDMIA GUARD: Allow WRAM and ROM. Block BIOS (0), MMIO (4), and EEPROM/SRAM (>= 0x0D)
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
				branchGuard3 = emitPtr;
				*emitPtr++ = PPC_BEQ(0);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 4);
				branchGuard1 = emitPtr;
				*emitPtr++ = PPC_BEQ(0);
				*emitPtr++ = PPC_CMPWI(0, PPC_R11, 13); // EEPROM BLOCK
				branchGuard2 = emitPtr;
				*emitPtr++ = PPC_BGE(0);
			}

			// 2. Load Page Pointer and Mask
			*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29);
			*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
			*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

			// 3. Null Pointer Guard
			*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
			u32* branchNullToBailout = emitPtr;
			*emitPtr++ = PPC_BEQ(0);

			// 4. Memory Operations Loop
			for (int i = 0; i < 8; i++) {
				if (regList & (1 << i)) {
					*emitPtr++ = PPC_AND(PPC_R4, PPC_R12, PPC_R11); // Apply Mask
					if (isLoad) *emitPtr++ = PPC_LWBRX(MapGBARegister(i), PPC_R10, PPC_R4);
					else        *emitPtr++ = PPC_STWBRX(MapGBARegister(i), PPC_R10, PPC_R4);
					*emitPtr++ = PPC_ADDI(PPC_R12, PPC_R12, 4); // ADVANCE R12 DIRECTLY
				}
			}

			// 5. Writeback to Base Register (Rn)
			// ARM protocol: If Rb is in the load list, the loaded value overrides writeback.
			bool writeback = true;
			if (isLoad && (regList & (1 << rb))) writeback = false;

			if (writeback) {
				*emitPtr++ = PPC_ADDI(MapGBARegister(rb), MapGBARegister(rb), numRegs * 4);
			}

			// 6. Branch out or Skip Bailout
			u32* branchSafe = emitPtr;
			*emitPtr++ = PPC_B(0);

			// 7. Bailout Stub Generation
			u32* bailoutTarget = emitPtr;
			*emitPtr++ = PPC_ADDI(PPC_R3, PPC_R3, chunkStaticCycles);
			*emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
			*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
			*emitPtr++ = PPC_BLR();

			// 8. Patch Branch Offsets
			if (branchGuard1) *branchGuard1 = (!isLoad) ? PPC_BNE((u32)((bailoutTarget - branchGuard1) * 4)) : PPC_BEQ((u32)((bailoutTarget - branchGuard1) * 4));
			if (branchGuard2) *branchGuard2 = PPC_BGE((u32)((bailoutTarget - branchGuard2) * 4));
			if (branchGuard3) *branchGuard3 = PPC_BEQ((u32)((bailoutTarget - branchGuard3) * 4));
			*branchNullToBailout = PPC_BEQ((u32)((bailoutTarget - branchNullToBailout) * 4));

			u32* safeTarget = emitPtr;
			*branchSafe = PPC_B((u32)((safeTarget - branchSafe) * 4));

			// thumbC0 (STMIA) / thumbC8 (LDMIA) both end with a plain assignment -
			// `clockTicks = (1 or 2) + codeTicksAccess16(armNextPC)` - which discards
			// every +1-per-register and per-register data-tick that THUMB_STM_REG/
			// THUMB_LDM_REG accumulated.
			// The real cost is just this flat constant + one code-fetch lookup;
			// `numRegs` never survives to be observable, so it's deliberately
			// excluded here rather than added (as it was before this fix).
			chunkStaticCycles += STATIC_CODE_TICKS_16(currentPC) + (isLoad ? 2 : 1);
		}
    	// THUMB Format 16 - Conditional Branches (Bcc)
		else if ((opcode & 0xF000) == 0xD000 && (opcode & 0x0F00) != 0x0F00) {
			u8 cond = (opcode >> 8) & 0x0F;
			s8 offset = (s8)(opcode & 0xFF);
			u32 targetPC = currentPC + 4 + (offset << 1);

			bool supported = false;
			bool isComposite = false;
			u32 flagReg = 0;
			bool branchIfSet = false;

			// Map native PowerPC hardware registers to GBA condition flags
			switch (cond) {
				case 0x0: flagReg = PPC_REG_Z; branchIfSet = true;  supported = true; break; // EQ (Z==1)
				case 0x1: flagReg = PPC_REG_Z; branchIfSet = false; supported = true; break; // NE (Z==0)
				case 0x2: flagReg = PPC_REG_C; branchIfSet = true;  supported = true; break; // CS (C==1)
				case 0x3: flagReg = PPC_REG_C; branchIfSet = false; supported = true; break; // CC (C==0)
				case 0x4: flagReg = PPC_REG_N; branchIfSet = true;  supported = true; break; // MI (N==1)
				case 0x5: flagReg = PPC_REG_N; branchIfSet = false; supported = true; break; // PL (N==0)
				case 0x6: flagReg = PPC_REG_V; branchIfSet = true;  supported = true; break; // VS (V==1)
				case 0x7: flagReg = PPC_REG_V; branchIfSet = false; supported = true; break; // VC (V==0)
				case 0x8: isComposite = true; branchIfSet = true;  supported = true; break;  // HI (C==1 & Z==0)
				case 0x9: isComposite = true; branchIfSet = false; supported = true; break;  // LS (C==0 | Z==1)
				case 0xA: isComposite = true; branchIfSet = true;  supported = true; break;  // GE (N==V)
				case 0xB: isComposite = true; branchIfSet = false; supported = true; break;  // LT (N!=V)
				case 0xC: isComposite = true; branchIfSet = true;  supported = true; break;  // GT (Z==0 & N==V)
				case 0xD: isComposite = true; branchIfSet = false; supported = true; break;  // LE (Z==1 | N!=V)
			}

			if (supported) {
				if (!isComposite) {
					// If branchIfSet == true, we skip the True Path when Flag == 0 (Equal to 0)
					// If branchIfSet == false, we skip the True Path when Flag == 1 (Not Equal to 0)
					*emitPtr++ = PPC_CMPWI(0, flagReg, 0);
					if (branchIfSet) *emitPtr++ = PPC_BEQ(20);
					else             *emitPtr++ = PPC_BNE(20);
				} else {
					bool branchIfZero = false;
					if (cond == 0x8 || cond == 0x9) {
						// HI takes branch if (C & ~Z) == 1. LS takes branch if (C & ~Z) == 0.
						*emitPtr++ = PPC_ANDC(PPC_R11, PPC_REG_C, PPC_REG_Z);
						branchIfZero = (cond == 0x9); // LS
					} else if (cond == 0xA || cond == 0xB) {
						// GE takes branch if (N ^ V) == 0. LT takes branch if (N ^ V) != 0.
						*emitPtr++ = PPC_XOR(PPC_R11, PPC_REG_N, PPC_REG_V);
						branchIfZero = (cond == 0xA); // GE
					} else if (cond == 0xC || cond == 0xD) {
						// GT takes branch if Z | (N ^ V) == 0. LE takes branch if Z | (N ^ V) != 0.
						*emitPtr++ = PPC_XOR(PPC_R11, PPC_REG_N, PPC_REG_V);
						*emitPtr++ = PPC_OR(PPC_R11, PPC_R11, PPC_REG_Z);
						branchIfZero = (cond == 0xC); // GT
					}
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 0);
					if (branchIfZero) *emitPtr++ = PPC_BNE(20); // Skip if R11 != 0
					else              *emitPtr++ = PPC_BEQ(20); // Skip if R11 == 0
				}

				// TRUE PATH (Branch Taken Exit)
				u32 takenPenalty = STATIC_CODE_TICKS_SEQ16(currentPC) + 1 +
				                           STATIC_CODE_TICKS_SEQ16(targetPC) + STATIC_CODE_TICKS_16(targetPC) + 2;

				EmitPrefetchSync(emitPtr, chunkInstrCount + 1, chunkStaticCycles + takenPenalty, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer
				*emitPtr++ = PPC_LIS(PPC_R4, targetPC >> 16);
				*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, targetPC & 0xFFFF);
				*emitPtr++ = PPC_BLR();

				// FALSE PATH (Branch Not Taken)
				chunkStaticCycles += STATIC_CODE_TICKS_SEQ16(currentPC + 2) + 1;
			} else {
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_CONDITIONAL_BRANCH);
				endBlock = true;
				break;
			}
		}
		// THUMB Formats 18 & 19 - Branch with Link (BL)
		else if ((opcode & 0xF800) == 0xF000) {
			u16 nextOpcode = CPUReadHalfWord(currentPC + 2);
			if ((nextOpcode & 0xF800) == 0xF800) {
				u32 offsetHigh = opcode & 0x07FF;
				u32 offsetLow = nextOpcode & 0x07FF;

				// Cast to signed BEFORE right shifting to force an arithmetic shift (sign extension)
				s32 sOffset = (s32)(offsetHigh << 21);
				sOffset >>= 9;
				sOffset |= (offsetLow << 1);

				u32 targetPC = currentPC + 4 + sOffset;

				// 1. Update LR (GBA R14) with the return address: (currentPC + 4) | 1
				u32 returnPC = (currentPC + 4) | 1;
				*emitPtr++ = PPC_LIS(MapGBARegister(14), returnPC >> 16);
				*emitPtr++ = PPC_ORI(MapGBARegister(14), MapGBARegister(14), returnPC & 0xFFFF);

				// 2. JIT EXIT: Branch Taken
				// PIPELINE SYNC: BL evaluates prefix at currentPC+2, and suffix at targetPC using N-cycle
				u32 takenPenalty = STATIC_CODE_TICKS_SEQ16(currentPC + 2) + 1 +
								   (STATIC_CODE_TICKS_SEQ16(targetPC) * 2) + STATIC_CODE_TICKS_16(targetPC) + 3;

				EmitPrefetchSync(emitPtr, chunkInstrCount + 1, chunkStaticCycles + takenPenalty, chunkStartPC);
				*emitPtr++ = PPC_LI(PPC_R5, 0); // Branch taken flushes prefetch buffer
				*emitPtr++ = PPC_LIS(PPC_R4, targetPC >> 16);
				*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, targetPC & 0xFFFF);
				*emitPtr++ = PPC_BLR();

				// BL is two THUMB halfwords (prefix + suffix), not one.
				instrCount += 2;
				currentPC += 4;
				endBlock = true;
				break;
			} else {
				JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_BRANCH_WITH_LINK);
				endBlock = true;
				break;
			}
		}
		else {
			JIT_LOG_BAILOUT(currentPC, opcode, BAILOUT_UNSUPPORTED_OPCODE);
			endBlock = true;
			break;
		}

		chunkInstrCount++;
		instrCount++;
		currentPC += 2;
    }

    if (instrCount == 0) {
    	cache.rewindJITMemory(MAX_WORDS * sizeof(u32));
        // Cache the failure! A stub block routes it directly to C++ without thrashing the compiler.
        BasicBlock* failBlock = new BasicBlock();
        failBlock->startPC = startPC;
        failBlock->length = 0;
        failBlock->execute = nullptr;
        cache.registerBlock(failBlock);
        return failBlock;
    }

    // Default Epilogue
    EmitPrefetchSync(emitPtr, chunkInstrCount, chunkStaticCycles, chunkStartPC);
	*emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
	*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
	*emitPtr++ = PPC_BLR();

    u32 emittedWords = (u32)(emitPtr - blockStart);
    u32 actualBytes = emittedWords * sizeof(u32);

    cache.rewindJITMemory((MAX_WORDS - emittedWords) * sizeof(u32));
    FlushJITCache(blockStart, actualBytes);

    BasicBlock* block = new BasicBlock();
    block->startPC = startPC;
    block->length = instrCount;
    block->execute = (JITBlockFunc)blockStart;

    cache.registerBlock(block);
    return block;
}

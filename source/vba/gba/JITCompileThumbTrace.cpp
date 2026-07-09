#include "JITCache.h"
#include "JITPPCEmitter.h"
#include "GBAinline.h"
#include "GBAcpu.h"
#include "JITProfiler.h"

static inline void FlushJITCache(void* addr, u32 size) {
    u32 start = (u32)addr & ~31;
    u32 end   = ((u32)addr + size + 31) & ~31;
    for (u32 i = start; i < end; i += 32) asm volatile("dcbst 0, %0" : : "r" (i) : "memory");
    asm volatile("sync" : : : "memory");
    for (u32 i = start; i < end; i += 32) asm volatile("icbi 0, %0" : : "r" (i) : "memory");
    asm volatile("sync \n isync" : : : "memory");
}

BasicBlock* JITCompileThumbTrace(u32 startPC, JITCache& cache) {
    u32* emitPtr = cache.allocateJITMemory(512 * sizeof(u32));
    u32* blockStart = emitPtr;

    u32 currentPC = startPC;
    u32 instrCount = 0;
    u32 staticCycles = 0; 
    bool endBlock = false;

	JIT_LOG_BLOCK_COMPILED();

    while (!endBlock && instrCount < 64) {
        // BUFFER OVERFLOW PROTECTION: Ensure we have enough words for the worst-case instruction (~20) + Epilogue
        if ((emitPtr - blockStart) > (512 - 32)) {
            endBlock = true;
            break;
        }

    	u16 opcode = CPUReadHalfWord(currentPC);

    	// THUMB Formats 1, 2, 3, 4 - Native ALU (Shifts, Add, Sub, Mov, Cmp)
		if ((opcode & 0xE000) == 0x0000 || (opcode & 0xFC00) == 0x4000) {
            // Format 3: Move Immediate (MOV Rd, #Imm8)
			if ((opcode & 0xF800) == 0x2000) {
				u8 rd = (opcode >> 8) & 0x07;
				u8 imm = opcode & 0xFF;
				*emitPtr++ = PPC_LI(MapGBARegister(rd), imm);
				*emitPtr++ = PPC_LI(PPC_REG_N, 0);
				*emitPtr++ = PPC_LI(PPC_REG_Z, (imm == 0) ? 1 : 0);
				staticCycles += codeTicksAccessSeq16(currentPC + 2) + 1;
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
				*emitPtr++ = PPC_RLWINM(PPC_REG_C, PPC_R11, 3, 31, 31); // IBM Bit 29 (CA) -> Bit 31
				*emitPtr++ = PPC_RLWINM(PPC_REG_V, PPC_R11, 2, 31, 31); // IBM Bit 30 (OV) -> Bit 31

				// Extract N and Z Flags natively (Branchless)
				*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31); // N = Rd >> 31
				*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));   // cntlzw
				*emitPtr++ = PPC_SRWI(PPC_REG_Z, PPC_REG_Z, 5);           // Z = (cntlzw == 32) ? 1 : 0

				staticCycles += codeTicksAccessSeq16(currentPC + 2) + 1;
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
                        // IBM Bit-Math: C = ARM bit (32 - offset) -> IBM bit (offset - 1). Rotate Left by (32 - offset).
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), 32 - offset, 31, 31);
						*emitPtr++ = PPC_RLWINM(MapGBARegister(rd), MapGBARegister(rs), offset, 0, 31 - offset);
					}
				}
				else if (op == 1) { // LSR
					if (offset == 0) {
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), 31, 31, 31); // LSR #0 is LSR #32 (C = ARM bit 31)
						*emitPtr++ = PPC_LI(MapGBARegister(rd), 0);
					} else {
                        // IBM Bit-Math: C = ARM bit (offset - 1) -> IBM bit (32 - offset). Rotate Left by (offset - 1).
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), offset - 1, 31, 31);
						*emitPtr++ = PPC_SRWI(MapGBARegister(rd), MapGBARegister(rs), offset);
					}
				}
                else if (op == 2) { // ASR
					if (offset == 0) {
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), 31, 31, 31); // ASR #0 is ASR #32 (C = ARM bit 31)
						*emitPtr++ = PPC_SRAWI(MapGBARegister(rd), MapGBARegister(rs), 31);
					} else {
						*emitPtr++ = PPC_RLWINM(PPC_REG_C, MapGBARegister(rs), offset - 1, 31, 31); // C = Last bit out
						*emitPtr++ = PPC_SRAWI(MapGBARegister(rd), MapGBARegister(rs), offset);
					}
                }

				*emitPtr++ = PPC_SRWI(PPC_REG_N, MapGBARegister(rd), 31);
				*emitPtr++ = PPC_CNTLZW(PPC_REG_Z, MapGBARegister(rd));
				*emitPtr++ = PPC_SRWI(PPC_REG_Z, PPC_REG_Z, 5);
				staticCycles += codeTicksAccessSeq16(currentPC + 2) + 1;
			}
            // Format 4: ALU Operations (Specifically CMP)
            else if ((opcode & 0xFC00) == 0x4000) {
                u8 op = (opcode >> 6) & 0x0F;
                u8 rs = (opcode >> 3) & 0x07;
                u8 rd = opcode & 0x07;

                if (op == 10) { // CMP (Compare)
                    // Compare is Subtraction without saving the result to Rd.
                    *emitPtr++ = PPC_SUBFCO(PPC_R12, MapGBARegister(rs), MapGBARegister(rd)); // R12 = Rd - Rs

                    // Hardware CA flag directly maps to GBA C_FLAG!
                    *emitPtr++ = PPC_MFXER(PPC_R11);
                    *emitPtr++ = PPC_RLWINM(PPC_REG_C, PPC_R11, 3, 31, 31);
                    *emitPtr++ = PPC_RLWINM(PPC_REG_V, PPC_R11, 2, 31, 31);

                    *emitPtr++ = PPC_SRWI(PPC_REG_N, PPC_R12, 31);
                    *emitPtr++ = PPC_CNTLZW(PPC_REG_Z, PPC_R12);
                    *emitPtr++ = PPC_SRWI(PPC_REG_Z, PPC_REG_Z, 5);

                    staticCycles += codeTicksAccessSeq16(currentPC + 2) + 1;
                } else {
                    endBlock = true;
                    break;
                }
            } else {
				endBlock = true;
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
            u32 takenPenalty = codeTicksAccessSeq16(currentPC + 2) + 1; // Base penalty

            *emitPtr++ = PPC_RLWINM(PPC_R4, PPC_R12, 0, 0, 30); // R4 = TargetPC & ~1
            *emitPtr++ = PPC_LI(PPC_R3, staticCycles + takenPenalty);
            *emitPtr++ = PPC_BLR();

            // FALSE PATH: ARM Switch Bailout
            u32* bailoutTarget = emitPtr;
            *emitPtr++ = PPC_LI(PPC_R3, staticCycles);
            *emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
            *emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
            *emitPtr++ = PPC_BLR();

            *branchArmSwitch = PPC_BEQ((u32)((bailoutTarget - branchArmSwitch) * 4));

            endBlock = true; // Block safely terminates here
            break;
        }

		// THUMB Format 6 - PC-Relative Load (LDR Rd, [PC, #Imm])
		else if ((opcode & 0xF800) == 0x4800) {
			u8 rd = (opcode >> 8) & 0x07;
			u8 imm = opcode & 0xFF;
			u32 targetAddr = ((currentPC + 4) & ~3) + (imm << 2);

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
				staticCycles += codeTicksAccessSeq16(currentPC + 2) + 2;
			} else {
				endBlock = true;
				break;
			}
		}

		// THUMB Formats 9, 10, 11 - Unified Memory Loads AND Stores
		else if (((opcode & 0xE000) == 0x6000) || ((opcode & 0xF000) == 0x5000) || ((opcode & 0xF000) == 0x8000)) {

			bool isMemLoad = false;
			bool isMemStore = false;
			u8 rd, rb, ro, imm;
			u32 accessType = 0; // 4=Word, 2=Halfword, 1=Byte

			// Format 9: LDR/STR Rd, [Rb, #Imm]
			if ((opcode & 0xF000) == 0x6000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; imm = (opcode >> 6) & 0x1F;
				*emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm << 2);
				accessType = 4;
				if (opcode & 0x0800) isMemLoad = true; else isMemStore = true;
			}
			// Format 9: LDRB/STRB Rd, [Rb, #Imm]
			else if ((opcode & 0xF000) == 0x7000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; imm = (opcode >> 6) & 0x1F;
				*emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm);
				accessType = 1;
				if (opcode & 0x0800) isMemLoad = true; else isMemStore = true;
			}
			// Format 11: LDRH/STRH Rd, [Rb, #Imm]
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
					break;
				}

				// 1. Extract Memory Bank (R12 >> 24)
				*emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);

				u32* branchGuard1 = nullptr;
				u32* branchGuard2 = nullptr;

				if (isMemStore) {
					// STORE STRICT GUARD: Only allow writes to Bank 0x02 (WRAM) & 0x03 (IRAM).
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
					*emitPtr++ = PPC_BEQ(12); // If Bank == 2, jump completely over the next two instructions
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
					branchGuard1 = emitPtr;
					*emitPtr++ = PPC_BNE(0);  // If Bank != 3 (and wasn't 2), bail out
				} else {
					// LOAD STRICT GUARD: Block Bank 0x04 (MMIO) and Bank >= 0x0E (Save Media)
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 4);
					branchGuard1 = emitPtr;
					*emitPtr++ = PPC_BEQ(0); // If Bank == 4, bail out
					// Bailout to Bank 14 (0x0E) to protect upper ROM boundaries
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 14);
					branchGuard2 = emitPtr;
					*emitPtr++ = PPC_BGE(0); // If Bank >= 14, bail out
				}

				// 2. Load Page Pointer and Mask
				*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29); // R11 = Bank * 4
				*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
				*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

				// 3. UNIVERSAL NULL POINTER GUARD (Prevents hardware DSI crashes)
				*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
				u32* branchNullToBailout = emitPtr;
				*emitPtr++ = PPC_BEQ(0);

				// 4. Apply Mask
				*emitPtr++ = PPC_AND(PPC_R12, PPC_R12, PPC_R11);

				// 5. Execute Memory Fetch or Store
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
				*emitPtr++ = PPC_LI(PPC_R3, staticCycles);
				*emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
				*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
				*emitPtr++ = PPC_BLR();

				// 8. Patch Branch Offsets
				if (branchGuard1) {
					*branchGuard1 = (isMemStore) ? PPC_BNE((u32)((bailoutTarget - branchGuard1) * 4)) : PPC_BEQ((u32)((bailoutTarget - branchGuard1) * 4));
				}
				if (branchGuard2) {
					*branchGuard2 = PPC_BGE((u32)((bailoutTarget - branchGuard2) * 4));
				}
				*branchNullToBailout = PPC_BEQ((u32)((bailoutTarget - branchNullToBailout) * 4));

				u32* safeTarget = emitPtr;
				*branchSafe = PPC_B((u32)((safeTarget - branchSafe) * 4));

				staticCycles += codeTicksAccessSeq16(currentPC + 2) + ((isMemStore) ? 2 : 3);
			} else {
				endBlock = true;
				break;
			}
		}

		// THUMB Format 16 - Conditional Branches (Bcc)
		else if ((opcode & 0xF000) == 0xD000 && (opcode & 0x0F00) != 0x0F00) {
			u8 cond = (opcode >> 8) & 0x0F;
			s8 offset = (s8)(opcode & 0xFF);
			u32 targetPC = currentPC + 4 + (offset << 1);

			bool supported = false;
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
			}

			if (supported) {
				*emitPtr++ = PPC_CMPWI(0, flagReg, 0);

				// If branchIfSet == true, we skip the True Path when Flag == 0 (Equal to 0)
				// If branchIfSet == false, we skip the True Path when Flag == 1 (Not Equal to 0)
				if (branchIfSet) *emitPtr++ = PPC_BEQ(20);
				else             *emitPtr++ = PPC_BNE(20);

				// TRUE PATH (Branch Taken Exit)
				// PIPELINE SYNC: Perfectly models the GBA prefetch penalty for taken branches
				u32 takenPenalty = codeTicksAccessSeq16(currentPC + 2) + 1 +
								   codeTicksAccessSeq16(targetPC) + codeTicksAccess16(targetPC) + 2;

				*emitPtr++ = PPC_LI(PPC_R3, staticCycles + takenPenalty);
				*emitPtr++ = PPC_LIS(PPC_R4, targetPC >> 16);
				*emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, targetPC & 0xFFFF);
				*emitPtr++ = PPC_BLR();

				// FALSE PATH (Branch Not Taken)
				staticCycles += codeTicksAccessSeq16(currentPC + 2) + 1;
			} else {
				// Bail to C++ for composite flags (HI, LS, GE, LT, GT, LE)
				endBlock = true;
				break;
			}
		}
		else {
			endBlock = true;
			break;
		}

        currentPC += 2;
        instrCount++;
    }

    if (instrCount == 0) {
        cache.rewindJITMemory(512 * sizeof(u32));
        // Cache the failure! A stub block routes it directly to C++ without thrashing the compiler.
        BasicBlock* failBlock = new BasicBlock();
        failBlock->startPC = startPC;
        failBlock->length = 0;
        failBlock->execute = nullptr;
        cache.registerBlock(failBlock);
        return failBlock;
    }

    // Default Epilogue
    *emitPtr++ = PPC_LI(PPC_R3, staticCycles);
    *emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
    *emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
    *emitPtr++ = PPC_BLR();

    u32 emittedWords = (u32)(emitPtr - blockStart);
    u32 actualBytes = emittedWords * sizeof(u32);

    cache.rewindJITMemory((512 - emittedWords) * sizeof(u32));
    FlushJITCache(blockStart, actualBytes);

    BasicBlock* block = new BasicBlock();
    block->startPC = startPC;
    block->length = instrCount;
    block->execute = (JITBlockFunc)blockStart;

    cache.registerBlock(block);
    return block;
}

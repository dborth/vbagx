#include "BlockCacheManager.h"
#include "GBAinline.h"
#include "GBAcpu.h"

static inline void FlushJITCache(void* addr, u32 size) {
    u32 start = (u32)addr & ~31;
    u32 end   = ((u32)addr + size + 31) & ~31;
    for (u32 i = start; i < end; i += 32) asm volatile("dcbst 0, %0" : : "r" (i) : "memory");
    asm volatile("sync" : : : "memory");
    for (u32 i = start; i < end; i += 32) asm volatile("icbi 0, %0" : : "r" (i) : "memory");
    asm volatile("sync \n isync" : : : "memory");
}

BasicBlock* CompileThumbTrace_JIT(u32 startPC, BlockCacheManager& cache) {
    u32* emitPtr = cache.allocateJITMemory(512 * sizeof(u32));
    u32* blockStart = emitPtr;

    u32 currentPC = startPC;
    u32 instrCount = 0;
    u32 staticCycles = 0; 
    bool endBlock = false;

    while (!endBlock && instrCount < 64) {
    	u16 opcode = CPUReadHalfWord(currentPC);

		// 1. THUMB Format 3 - Move Immediate (MOV Rd, #Imm8)
		if ((opcode & 0xF800) == 0x2000) {
			u8 rd = (opcode >> 8) & 0x07;
			u8 imm = opcode & 0xFF;
			*emitPtr++ = PPC_LI(MapGBARegister(rd), imm);
			*emitPtr++ = PPC_LI(PPC_REG_N, 0);
			*emitPtr++ = PPC_LI(PPC_REG_Z, (imm == 0) ? 1 : 0);

			// Use currentPC + 2 to perfectly map C++ 'oldArmNextPC' timing sync
			staticCycles += codeTicksAccessSeq16(currentPC + 2) + 1;
		}

		// 2. THUMB Format 6 - PC-Relative Load (LDR Rd, [PC, #Imm])
		else if ((opcode & 0xF800) == 0x4800) {
			u8 rd = (opcode >> 8) & 0x07;
			u8 imm = opcode & 0xFF;
			u32 targetAddr = ((currentPC + 4) & ~3) + (imm << 2);

			u8 bank = targetAddr >> 24;
			// ONLY statically bake BIOS (0x00) and ROM (0x08-0x0C).
			// Bank 0x0D+ contains SRAM/Registers which mutate!
			if (bank == 0x00 || (bank >= 0x08 && bank <= 0x0C)) {
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
		// 3. THUMB Formats 9, 10, 11 - Unified Memory Loads AND Stores
		else if (((opcode & 0xE000) == 0x6000) || ((opcode & 0xF000) == 0x5000) || ((opcode & 0xF000) == 0x8000)) {

			bool isMemLoad = false;
			bool isMemStore = false;
			u8 rd, rb, ro, imm;
			u32 accessType = 0; // 4=Word, 2=Halfword, 1=Byte

			// Format 9: LDR/STR Rd, [Rb, #Imm]
			if ((opcode & 0xE800) == 0x6000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; imm = (opcode >> 6) & 0x1F;
				*emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm << 2);
				accessType = 4;
				if (opcode & 0x0800) isMemLoad = true; else isMemStore = true;
			}
			// Format 9: LDRB/STRB Rd, [Rb, #Imm]
			else if ((opcode & 0xE800) == 0x7000) {
				rd = opcode & 0x07; rb = (opcode >> 3) & 0x07; imm = (opcode >> 6) & 0x1F;
				*emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm);
				accessType = 1;
				if (opcode & 0x0800) isMemLoad = true; else isMemStore = true;
			}
			// Format 11: LDRH/STRH Rd, [Rb, #Imm]
			else if ((opcode & 0xE800) == 0x8000) {
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

				u32* branchStoreInvalid = nullptr;
				if (isMemStore) {
					// STORE STRICT GUARD: Only allow writes to Bank 0x02 (WRAM) & 0x03 (IRAM).
					// This perfectly protects ROM, VRAM Mirrors, and Save Flash from JIT corruption.
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 2);
					*emitPtr++ = PPC_BEQ(8); // If == 2, skip the BNE failure
					*emitPtr++ = PPC_CMPWI(0, PPC_R11, 3);
					branchStoreInvalid = emitPtr;
					*emitPtr++ = PPC_BNE(0); // If != 3, bail out to C++
				}

				// 2. Load Page Pointer and Mask
				*emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29); // R11 = Bank * 4
				*emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
				*emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);

				// 3. UNIVERSAL NULL POINTER GUARD (Prevents hardware DSI crashes)
				*emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
				u32* branchNullToBailout = emitPtr;
				*emitPtr++ = PPC_BEQ(0); // Patched below

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
				*branchNullToBailout = PPC_BEQ((u32)((bailoutTarget - branchNullToBailout) * 4));
				if (branchStoreInvalid) {
					*branchStoreInvalid = PPC_BNE((u32)((bailoutTarget - branchStoreInvalid) * 4));
				}

				u32* safeTarget = emitPtr;
				*branchSafe = PPC_B((u32)((safeTarget - branchSafe) * 4));

				staticCycles += codeTicksAccessSeq16(currentPC + 2) + 2;
			} else {
				endBlock = true;
				break;
			}
		}

		// 4. THUMB Format 16 - Conditional Branches (Bcc)
		else if ((opcode & 0xF000) == 0xD000 && (opcode & 0x0F00) != 0x0F00) {
			u8 cond = (opcode >> 8) & 0x0F;
			s8 offset = (s8)(opcode & 0xFF);
			u32 targetPC = currentPC + 4 + (offset << 1);

			if (cond == 0x0 || cond == 0x1) {
				*emitPtr++ = PPC_CMPWI(0, PPC_REG_Z, 0);

				// BEQ/BNE skip over the 4 instructions of the True Path
				if (cond == 0x0) *emitPtr++ = PPC_BEQ(20);
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

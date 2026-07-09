#include "BlockCacheManager.h"
#include "GBAinline.h"

extern u16 CPUReadHalfWord(u32 address);
extern u32 CPUReadMemory(u32 address);
extern int codeTicksAccessSeq16(u32 address);

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

            staticCycles += codeTicksAccessSeq16(currentPC) + 1;
        }

        // 2. THUMB Format 6 - PC-Relative Load (LDR Rd, [PC, #Imm])
        else if ((opcode & 0xF800) == 0x4800) {
            u8 rd = (opcode >> 8) & 0x07;
            u8 imm = opcode & 0xFF;
            u32 targetAddr = ((currentPC + 4) & ~3) + (imm << 2);

            u8 bank = targetAddr >> 24;
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
                staticCycles += codeTicksAccessSeq16(currentPC) + 2;
            } else {
                endBlock = true;
                break;
            }
        }

        // 3. THUMB Format 9 - LDR Rd, [Rb, #Imm]
        else if ((opcode & 0xF800) == 0x6800) {
            // Prevent Infinite Bailout Loop
            // Refuse to compile a dynamic memory load if it's the very first instruction.
            // This safely hands execution back to the C++ fallback to advance the PC past the MMIO hazard.
            if (instrCount == 0) {
                endBlock = true;
                break;
            }

            u8 rd = opcode & 0x07;
            u8 rb = (opcode >> 3) & 0x07;
            u8 imm = (opcode >> 6) & 0x1F;

            *emitPtr++ = PPC_ADDI(PPC_R12, MapGBARegister(rb), imm << 2);
            *emitPtr++ = PPC_SRWI(PPC_R11, PPC_R12, 24);
            *emitPtr++ = PPC_RLWINM(PPC_R11, PPC_R11, 2, 0, 29);

            *emitPtr++ = PPC_LWZX(PPC_R10, PPC_R30_PAGES, PPC_R11);
            *emitPtr++ = PPC_CMPWI(0, PPC_R10, 0);
            *emitPtr++ = PPC_BNE(20);

            // FALSE PATH (I/O Register Bailout)
            *emitPtr++ = PPC_LI(PPC_R3, staticCycles);
            *emitPtr++ = PPC_LIS(PPC_R4, currentPC >> 16);
            *emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, currentPC & 0xFFFF);
            *emitPtr++ = PPC_BLR();

            // TRUE PATH (Valid Read)
            *emitPtr++ = PPC_LWZX(PPC_R11, PPC_R31_MASKS, PPC_R11);
            *emitPtr++ = PPC_AND(PPC_R12, PPC_R12, PPC_R11);
            *emitPtr++ = PPC_LWBRX(MapGBARegister(rd), PPC_R10, PPC_R12);

            staticCycles += codeTicksAccessSeq16(currentPC) + 2;
        }

        // 4. THUMB Format 16 - Conditional Branches (Bcc)
        else if ((opcode & 0xF000) == 0xD000 && (opcode & 0x0F00) != 0x0F00) {
            u8 cond = (opcode >> 8) & 0x0F;
            s8 offset = (s8)(opcode & 0xFF);
            u32 targetPC = currentPC + 4 + (offset << 1);

            if (cond == 0x0 || cond == 0x1) {
                // Always compare to 0. C++ boolean flags might not be exactly 1.
                *emitPtr++ = PPC_CMPWI(0, PPC_REG_Z, 0);

                if (cond == 0x0) *emitPtr++ = PPC_BEQ(20); // BEQ: If Z==0 (False), skip True Path
                else             *emitPtr++ = PPC_BNE(20); // BNE: If Z!=0 (True), skip True Path

                // TRUE PATH (Branch Taken Exit)
                *emitPtr++ = PPC_LI(PPC_R3, staticCycles + 1);
                *emitPtr++ = PPC_LIS(PPC_R4, targetPC >> 16);
                *emitPtr++ = PPC_ORI(PPC_R4, PPC_R4, targetPC & 0xFFFF);
                *emitPtr++ = PPC_BLR();

                // FALSE PATH (Branch Not Taken)
                staticCycles += codeTicksAccessSeq16(currentPC) + 1;
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

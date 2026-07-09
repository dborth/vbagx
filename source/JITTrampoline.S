.global ExecuteJITTrace
.type ExecuteJITTrace, @function

# -----------------------------------------------------------------------------
# ExecuteJITTrace(JITBlockFunc execute, u32* gbaRegs, u32* flags,
#                 JITResult* outResult, u8** readPages, u32* readMasks)
#
# Register Allocations on Entry (PowerPC Standard EABI):
#   R3: execute (Function pointer to JIT block)
#   R4: gbaRegs (Pointer to GBA registers array, reg[0..15])
#   R5: flags (Pointer to flag buffer array)
#   R6: outResult (Pointer to JITResult struct)
#   R7: readPages (Pointer to gbaReadPagePtrs)
#   R8: readMasks (Pointer to gbaReadPageMasks)
#
# JIT Execution Register Mapping (from BlockCacheManager.h):
#   R14..R29 : GBA Registers R0..R15
#   R30      : PPC_R30_PAGES (readPages base array pointer)
#   R31      : PPC_R31_MASKS (readMasks base array pointer)
#   R8       : PPC_REG_N (N_FLAG)
#   R9       : PPC_REG_Z (Z_FLAG)
# -----------------------------------------------------------------------------

ExecuteJITTrace:
    # 1. Save Link Register (LR) in caller's stack frame
    mflr 0
    stw 0, 4(1)

    # 2. Allocate Stack Frame (128 bytes) & Save Host Non-Volatile Registers (R14-R31)
    stwu 1, -128(1)
    stmw 14, 8(1)    # Save host registers r14-r31 to stack frame (offsets 8 to 79)

    # 3. Preserve Volatile Arguments to Stack Frame
    stw 4, 80(1)     # Stack[80] = gbaRegs pointer
    stw 5, 84(1)     # Stack[84] = flags pointer
    stw 6, 88(1)     # Stack[88] = outResult pointer

    # 4. Load ALL 16 GBA Registers (R0-R15) individually into Hardware R14-R29
    # Explicitly load to prevent clobbering R30/R31 and out-of-bounds reads
    lwz 14, 0(4)
    lwz 15, 4(4)
    lwz 16, 8(4)
    lwz 17, 12(4)
    lwz 18, 16(4)
    lwz 19, 20(4)
    lwz 20, 24(4)
    lwz 21, 28(4)
    lwz 22, 32(4)
    lwz 23, 36(4)
    lwz 24, 40(4)
    lwz 25, 44(4)
    lwz 26, 48(4)
    lwz 27, 52(4)
    lwz 28, 56(4)
    lwz 29, 60(4)   # R29 = GBA R15 (PC)

    # 5. Load Global Page/Mask Pointers safely into Non-Volatile R30 / R31
    mr 30, 7        # R30 = readPages (PPC_R30_PAGES)
    mr 31, 8        # R31 = readMasks (PPC_R31_MASKS)

    # 6. Load Flags into Dedicated JIT Flag Registers (R8 = N_FLAG, R9 = Z_FLAG)
    lwz 8, 0(5)     # R8 = N_FLAG (PPC_REG_N)
    lwz 9, 4(5)     # R9 = Z_FLAG (PPC_REG_Z)

    # 7. Execute JIT Block Trace
    mtctr 3         # Move JIT block function pointer to Count Register (CTR)
    bctrl           # Branch to CTR and Link (Returns: R3 = cycles, R4 = nextPC)

    # 8. Post-Execution: Write Back Return Values and GBA CPU State
    # CRITICAL FIX: Reload original pointers since volatile registers were overwritten

    # Save JIT execution results to outResult struct
    lwz 6, 88(1)    # R6 = outResult pointer
    stw 3, 0(6)     # outResult->cycles = R3
    stw 4, 4(6)     # outResult->nextPC = R4

    # Save JIT Flag Registers back to C++ flagBuffer
    lwz 5, 84(1)    # R5 = flags pointer
    stw 8, 0(5)     # flagBuffer[0] = N_FLAG (R8)
    stw 9, 4(5)     # flagBuffer[1] = Z_FLAG (R9)

    # Save GBA Registers R0-R15 back to gbaRegs
    # CRITICAL FIX: Explicitly store to prevent writing past the array bounds
    lwz 10, 80(1)   # R10 = gbaRegs pointer
    stw 14, 0(10)
    stw 15, 4(10)
    stw 16, 8(10)
    stw 17, 12(10)
    stw 18, 16(10)
    stw 19, 20(10)
    stw 20, 24(10)
    stw 21, 28(10)
    stw 22, 32(10)
    stw 23, 36(10)
    stw 24, 40(10)
    stw 25, 44(10)
    stw 26, 48(10)
    stw 27, 52(10)
    stw 28, 56(10)
    stw 29, 60(10)

    # 9. Clean up Stack Frame and Restore Host Non-Volatile Registers
    lmw 14, 8(1)    # Restore host registers r14-r31
    lwz 0, 128+4(1) # Load saved host Link Register
    mtlr 0          # Restore LR
    addi 1, 1, 128  # Collapse stack frame
    blr             # Return to C++ host loop

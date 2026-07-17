.global ExecuteJITTrace
.type ExecuteJITTrace, @function

# -----------------------------------------------------------------------------
# ExecuteJITTrace(JITBlockFunc execute, u32* gbaRegs, u32* flags,
#                 JITResult* outResult, u8** readPages, u32* readMasks,
#                 u32* busPrefetchCount)
#
# Register Allocations on Entry (PowerPC Standard EABI):
#   R3: execute (Function pointer to JIT block)
#   R4: gbaRegs (Pointer to GBA registers array, reg[0..15])
#   R5: flags (Pointer to flag buffer array) -- CONSUMED early; see note below
#   R6: outResult (Pointer to JITResult struct)
#   R7: readPages (Pointer to gbaReadPagePtrs)
#   R8: readMasks (Pointer to gbaReadPageMasks)
#   R9: busPrefetchCount (Pointer to the live hardware-prefetch-buffer state,
#       shared with the C++ interpreter loop -- read in, written back out,
#       so JIT'd and interpreted execution see a consistent prefetch model)
#
# JIT Execution Register Mapping (from JITCache.h / JITPPCEmitter.h):
#   R14..R29 : GBA Registers R0..R15
#   R30      : PPC_R30_PAGES (readPages base array pointer)
#   R31      : PPC_R31_MASKS (readMasks base array pointer)
#   R8       : PPC_REG_N (N_FLAG)
#   R9       : PPC_REG_Z (Z_FLAG)
#   R6       : PPC_REG_C (C_FLAG)
#   R7       : PPC_REG_V (V_FLAG)
#   R5       : PPC_R5 -- live busPrefetchCount accumulator DURING the trace.
#       NOTE the reuse: R5 is the `flags` pointer argument on entry, but only
#       until the four lwz's below have drained it into R8/R9/R6/R7. After
#       that point R5 is repointed to hold the *value* at *busPrefetchCount
#       (loaded via R9, the 7th arg) for the duration of bctrl, since the
#       compiled trace itself reads/writes R5 as its prefetch-buffer state
#       (see EmitPrefetchSync/EmitPrefetchDataWait in JITCompileThumbTrace.cpp).
#       R9 is likewise dual-purpose: incoming busPrefetchCount pointer at
#       entry, later reloaded from the stack as scratch to write the final
#       R5 value back out. Neither reuse is accidental, but both are easy to
#       misread without this note -- if you're adding a new live-register
#       accumulator here, follow this same drain-then-repurpose pattern and
#       document it the same way.
# -----------------------------------------------------------------------------

ExecuteJITTrace:
    # 1. Save Link Register (LR) in caller's stack frame
    mflr 0
    stw 0, 4(1)

    # 2. Allocate Stack Frame (128 bytes) & Save Host Non-Volatile Registers (R14-R31)
    stwu 1, -128(1)
    stmw 14, 8(1)    # Save host registers r14-r31 to stack frame (offsets 8 to 79)

    # 3. Preserve Volatile Arguments to Stack Frame
    stw 4, 80(1)     # Stack[80] = gbaRegs
    stw 5, 84(1)     # Stack[84] = flags array
    stw 6, 88(1)     # Stack[88] = outResult
    stw 9, 92(1)     # Stack[92] = busPrefetchCount pointer (7th EABI argument)

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

    # 6. Load ALL 4 Flags into Dedicated JIT Registers
    lwz 8, 0(5)     # R8 = N_FLAG (PPC_REG_N)
    lwz 9, 4(5)     # R9 = Z_FLAG (PPC_REG_Z)
    lwz 6, 8(5)     # R6 = C_FLAG (PPC_REG_C)
    lwz 7, 12(5)    # R7 = V_FLAG (PPC_REG_V)

    # Load live busPrefetchCount into R5
    lwz 10, 92(1)    # R10 = busPrefetchCount pointer
    lwz 5, 0(10)     # R5 = *busPrefetchCount

    # --- NORMALIZE C++ FLAGS TO STRICT 0 OR 1 ---
    # Normalize N_FLAG
    cmpwi 8, 0
    li 8, 0
    beq 1f
    li 8, 1
1:
    # Normalize Z_FLAG
    cmpwi 9, 0
    li 9, 0
    beq 2f
    li 9, 1
2:
    # Normalize C_FLAG
    cmpwi 6, 0
    li 6, 0
    beq 3f
    li 6, 1
3:
    # Normalize V_FLAG
    cmpwi 7, 0
    li 7, 0
    beq 4f
    li 7, 1
4:
    # 7. Execute JIT Block Trace
    mtctr 3         # Move JIT block function pointer to Count Register (CTR)
    li 3, 0         # Zero out the hardware cycle accumulator
    bctr            # Branch to CTR (Returns natively via ExecuteJITTrace_Return)

    # -------------------------------------------------------------------------
    # LINKER STUB MISS LANDING PAD
    # The JIT Cache Linker jumps here when a native Block-to-Block hash lookup
    # fails, cleanly returning execution to the C++ host.
    # -------------------------------------------------------------------------
    .global ExecuteJITTrace_Return
ExecuteJITTrace_Return:

    # 8. Post-Execution: Write Back Return Values
    lwz 10, 92(1)   # R10 = busPrefetchCount pointer
    stw 5, 0(10)    # *busPrefetchCount = R5
    lwz 5, 84(1)    # Reload flags array pointer
    stw 8, 0(5)
    stw 9, 4(5)
    stw 6, 8(5)
    stw 7, 12(5)

    # Reload outResult AFTER saving C_FLAG (which lived in R6)
    lwz 6, 88(1)
    stw 3, 0(6)     # outResult->cycles
    stw 4, 4(6)     # outResult->nextPC

    # Save GBA Registers R0-R15 back to gbaRegs
    # Explicitly store to prevent writing past the array bounds
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

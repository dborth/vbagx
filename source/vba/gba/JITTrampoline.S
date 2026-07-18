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
#   R5: flags (Pointer to flag buffer array)
#   R6: outResult (Pointer to JITResult struct)
#   R7: readPages (Pointer to gbaReadPagePtrs)
#   R8: readMasks (Pointer to gbaReadPageMasks)
#   R9: busPrefetchCount (Pointer to the live hardware-prefetch-buffer state)
#
# JIT Execution Register Mapping (Lazy Register Paradigm):
#   R14      : PPC_REG_GBA_REGS_PTR (Preserved pointer to the C++ gbaRegs array)
#   R15..R28 : Lazily allocated/cached GBA Registers R0..R14
#   R29      : PPC_REG_PC (GBA R15 / Pipeline PC - synchronized across boundaries)
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
    stmw 14, 8(1)    # Save host registers r14-r31 to stack frame

    # 3. Preserve Volatile Arguments to Stack Frame
    stw 4, 80(1)     # Stack[80] = gbaRegs pointer
    stw 5, 84(1)     # Stack[84] = flags array pointer
    stw 6, 88(1)     # Stack[88] = outResult pointer
    stw 9, 92(1)     # Stack[92] = busPrefetchCount pointer

    # 4. Initialize Lazy Register Context Pointers
    mr 14, 4         # R14 = PPC_REG_GBA_REGS_PTR (Keep base pointer alive for lazy loads/stores)
    lwz 29, 60(4)    # R29 = GBA R15 (PC) is loaded eagerly to maintain block branch pipeline tracking

    # 5. Load Global Page/Mask Pointers safely into Non-Volatile R30 / R31
    mr 30, 7         # R30 = readPages (PPC_R30_PAGES)
    mr 31, 8         # R31 = readMasks (PPC_R31_MASKS)

    # 6. Load ALL 4 Flags into Dedicated JIT Registers
    lwz 8, 0(5)      # R8 = N_FLAG
    lwz 9, 4(5)      # R9 = Z_FLAG
    lwz 6, 8(5)      # R6 = C_FLAG
    lwz 7, 12(5)     # R7 = V_FLAG

    # Load live busPrefetchCount into R5
    lwz 10, 92(1)    # R10 = busPrefetchCount pointer
    lwz 5, 0(10)     # R5 = *busPrefetchCount

    # --- NORMALIZE C++ FLAGS TO STRICT 0 OR 1 ---
    cmpwi 8, 0
    li 8, 0
    beq 1f
    li 8, 1
1:
    cmpwi 9, 0
    li 9, 0
    beq 2f
    li 9, 1
2:
    cmpwi 6, 0
    li 6, 0
    beq 3f
    li 6, 1
3:
    cmpwi 7, 0
    li 7, 0
    beq 4f
    li 7, 1
4:
    # 7. Execute JIT Block Trace
    mtctr 3          # Move JIT block function pointer to Count Register (CTR)
    li 3, 0          # Zero out the hardware cycle accumulator
    bctr             # Branch to CTR

    # -------------------------------------------------------------------------
    # LINKER STUB MISS LANDING PAD
    # -------------------------------------------------------------------------
    .global ExecuteJITTrace_Return
ExecuteJITTrace_Return:

    # 8. Post-Execution: Write Back Return Values
    lwz 10, 92(1)    # R10 = busPrefetchCount pointer
    stw 5, 0(10)     # *busPrefetchCount = R5
    lwz 5, 84(1)     # Reload flags array pointer
    stw 8, 0(5)
    stw 9, 4(5)
    stw 6, 8(5)
    stw 7, 12(5)

    # Reload outResult AFTER saving C_FLAG (which lived in R6)
    lwz 6, 88(1)
    stw 3, 0(6)      # outResult->cycles
    stw 4, 4(6)      # outResult->nextPC

    # Eagerly flush the modified pipeline PC back to the register state
    lwz 10, 80(1)    # R10 = gbaRegs pointer
    stw 29, 60(10)   # Synchronize GBA R15 (PC)

    # NOTE: All other GBA registers (R0-R14) must be lazily flushed by the
    # JIT trace prior to reaching an exit point or dynamic bailout path.

    # 9. Clean up Stack Frame and Restore Host Non-Volatile Registers
    lmw 14, 8(1)     # Restore host registers r14-r31
    lwz 0, 128+4(1)  # Load saved host Link Register
    mtlr 0           # Restore LR
    addi 1, 1, 128   # Collapse stack frame
    blr              # Return to C++ host loop

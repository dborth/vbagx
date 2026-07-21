.global ExecuteJITTrace
.type ExecuteJITTrace, @function

# -----------------------------------------------------------------------------
# ExecuteJITTrace(JITBlockFunc execute, JITResult* outResult,
#                 u32* busPrefetchCount, u32* gbaRegs, u32* flags,
#                 u8** readPages, u32* readMasks)
#
# Register Allocations on Entry (PowerPC Standard EABI):
#   R3: execute (Function pointer to JIT block)
#   R4: outResult (Pointer to JITResult struct)
#   R5: busPrefetchCount (Pointer to the live hardware-prefetch-buffer state)
#   R6: gbaRegs (Pointer to GBA registers array, reg[0..15])
#   R7: flags (Pointer to flag buffer array)
#   R8: readPages (Pointer to gbaReadPagePtrs)
#   R9: readMasks (Pointer to gbaReadPageMasks)
#
# JIT Execution Register Mapping (Lazy Register Paradigm):
#   R14      : PPC_REG_GBA_REGS_PTR (Preserved pointer to the C++ gbaRegs array)
#   R15..R28 : Lazily allocated/cached GBA Registers R0..R14
#   R29      : PPC_REG_PC (GBA R15 / Pipeline PC - synchronized across boundaries)
#   R30      : PPC_R30_PAGES (readPages base array pointer)
#   R31      : PPC_R31_MASKS (readMasks base array pointer)
#   R8       : PPC_REG_N (N_FLAG) - Lazily Allocated
#   R9       : PPC_REG_Z (Z_FLAG) - Lazily Allocated
#   R6       : PPC_REG_C (C_FLAG) - Lazily Allocated
#   R7       : PPC_REG_V (V_FLAG) - Lazily Allocated
#   R5       : PPC_R5 -- live busPrefetchCount accumulator DURING the trace
# -----------------------------------------------------------------------------

ExecuteJITTrace:
    # Save Link Register (LR) in caller's stack frame
    mflr 0
    stw 0, 4(1)

    # Allocate Stack Frame (128 bytes) & Save Host Non-Volatile Registers (R14-R31)
    stwu 1, -128(1)
    stmw 14, 8(1)    # Save host registers r14-r31 to stack frame

    # Preserve Volatile Arguments to Stack Frame
    stw 6, 80(1)     # Stack[80] = gbaRegs pointer (R6)
    stw 7, 84(1)     # Stack[84] = flags array pointer (R7)
    stw 4, 88(1)     # Stack[88] = outResult pointer (R4)
    stw 5, 92(1)     # Stack[92] = busPrefetchCount pointer (R5)

    # Initialize Lazy Register Context Pointers
    mr 14, 6         # R14 = PPC_REG_GBA_REGS_PTR (Keep base pointer alive for lazy loads/stores)
    lwz 29, 60(6)    # R29 = GBA R15 (PC) is loaded eagerly to maintain block branch pipeline tracking

    # Load Global Page/Mask Pointers safely into Non-Volatile R30 / R31
    mr 30, 8         # R30 = readPages (PPC_R30_PAGES)
    mr 31, 9         # R31 = readMasks (PPC_R31_MASKS)

    # Load live busPrefetchCount into R5 (safely fetched from the pointer)
    lwz 10, 92(1)    # R10 = busPrefetchCount pointer
    lwz 5, 0(10)     # R5 = *busPrefetchCount

    # Execute JIT Block Trace
    mtctr 3          # Move JIT block function pointer to Count Register (CTR)
    li 3, 0          # Zero out the hardware cycle accumulator
    bctr             # Branch to CTR

    # -------------------------------------------------------------------------
    # LINKER STUB MISS LANDING PAD
    # -------------------------------------------------------------------------
    .global ExecuteJITTrace_Return
ExecuteJITTrace_Return:

    # Post-Execution: Write Back Return Values
    lwz 10, 92(1)    # R10 = busPrefetchCount pointer
    stw 5, 0(10)     # *busPrefetchCount = R5

    # Reload outResult (Using safe scratch R10 to preserve trace outputs)
    lwz 10, 88(1)    # R10 = outResult pointer
    stw 3, 0(10)     # outResult->cycles (Returned by JIT trace in R3)
    stw 4, 4(10)     # outResult->nextPC (Returned by JIT trace in R4)

    # Eagerly flush the modified pipeline PC back to the register state
    lwz 10, 80(1)    # R10 = gbaRegs pointer
    stw 29, 60(10)   # Synchronize GBA R15 (PC)

    # NOTE: All GBA flags and registers (R0-R14) must be lazily flushed by the
    # JIT trace prior to reaching an exit point or dynamic bailout path.

    # Clean up Stack Frame and Restore Host Non-Volatile Registers
    lmw 14, 8(1)     # Restore host registers r14-r31
    lwz 0, 128+4(1)  # Load saved host Link Register
    mtlr 0           # Restore LR
    addi 1, 1, 128   # Collapse stack frame
    blr              # Return to C++ host loop

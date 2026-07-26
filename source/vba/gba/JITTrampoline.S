/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * JITTrampoline.S
 *
 * Hand-written PowerPC assembly ABI bridge between the C++ emulator core
 * and compiled JIT traces. This is the only place that translates the
 * ExecuteJITTrace(...) C calling convention into the JIT's internal,
 * non-standard register contract (see JITPPCEmitter.h's register map).
 *
 * ExecuteJITTrace:
 *   - Saves the caller's LR and all non-volatile host registers (R14-R31)
 *     to a fresh 128-byte stack frame, since compiled traces treat R14,
 *     R29-R31 as long-lived state for the duration of the trace.
 *   - Stashes the incoming pointer arguments (gbaRegs, flags, outResult,
 *     busPrefetchCount) into the stack frame at fixed offsets, since the
 *     JIT-emitted code reaches back into this frame (e.g. flags at 84(r1),
 *     outResult at 88(r1)) to load/store lazily rather than carrying every
 *     pointer in a live register for the whole trace.
 *   - Eagerly loads GBA R15 (PC) into R29 and the live busPrefetchCount
 *     value into R5 — the two pieces of state that must already be resident
 *     before the very first compiled instruction runs, since they can't be
 *     lazily faulted in the way GBA R0-R14 and the condition flags are.
 *   - Zeroes the R3 cycle accumulator, then transfers control into the
 *     compiled block via mtctr/bctr (never a normal call — the trace may
 *     chain through any number of subsequent already-compiled blocks via
 *     JITCache's self-patching linker stub before it ever comes back here).
 *
 * ExecuteJITTrace_Return:
 *   - The single landing pad every JIT exit path (default epilogue, deferred
 *     bailout, SMC bailout, quota-shield yield, or a genuine linker-stub
 *     cache miss) ultimately branches to. Writes R3 (cycles) / R4 (nextPC)
 *     out to outResult, flushes the live busPrefetchCount and GBA PC back
 *     to memory, restores the saved host non-volatile registers and LR,
 *     and returns to the C++ dispatch loop.
 *   - Does NOT flush GBA R0-R14 or the condition flags — by contract, any
 *     code that reaches this landing pad has already flushed whatever it
 *     left dirty (FlushDirtyRegisters/FlushDirtyFlags in JITCompiler.cpp);
 *     this trampoline only owns the handful of pieces of state it itself
 *     eagerly loaded on entry.
 ***************************************************************************/

.global ExecuteJITTrace
.type ExecuteJITTrace, @function

# -----------------------------------------------------------------------------
# ExecuteJITTrace(JITBlockFunc execute, JITResult* outResult,
#                 u32* busPrefetchCount, u32* gbaRegs, u32* flags,
#                 GBAReadPageTable readTable)
#
# Register Allocations on Entry (PowerPC Standard EABI):
#   R3: execute (Function pointer to JIT block)
#   R4: outResult (Pointer to JITResult struct)
#   R5: busPrefetchCount (Pointer to the live hardware-prefetch-buffer state)
#   R6: gbaRegs (Pointer to GBA registers array, reg[0..15])
#   R7: flags (Pointer to flag buffer array)
#   R8: gbaReadTable (Pointer to gbaReadTable)
#
# JIT Execution Register Mapping (Lazy Register Paradigm):
#   R14      : PPC_REG_GBA_REGS_PTR (Preserved pointer to the C++ gbaRegs array)
#   R15..R28 : Lazily allocated/cached GBA Registers R0..R14
#   R29      : PPC_REG_PC (GBA R15 / Pipeline PC - synchronized across boundaries)
#   R30      : PPC_R30_TABLE (readTable base pointer)
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

    # Load Global gbaReadTable Pointer safely into Non-Volatile R30
    mr 30, 8         # R30 = gbaReadTable (PPC_R30_TABLE)

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

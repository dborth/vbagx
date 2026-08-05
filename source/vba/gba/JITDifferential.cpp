/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * JITDifferential.cpp
 *
 * Implements the differential mismatch-detection loop (only compiled when
 * JIT_DIFFERENTIAL_TESTING is defined). For a given block, this:
 *   1. Snapshots the full CPU state (all 16 registers, flags, armNextPC,
 *      cpuTotalTicks, prefetch buffer state) before anything runs.
 *   2. Runs the compiled JIT trace via ExecuteJITTrace() and records its
 *      reported cycles/nextPC/flags/registers.
 *   3. Restores the *pre-JIT* snapshot, then re-executes the same guest
 *      instructions through the plain C++ interpreter one at a time
 *      (thumbInsnTable[]) until it has burned the same number of cycles
 *      the JIT reported (or hits a scheduler/event boundary), as an
 *      independent "should have happened" reference run.
 *   4. Compares the two outcomes register-by-register, flag-by-flag, plus
 *      next PC and total cycles. Any mismatch is written to the shared
 *      debug log via LogJITMismatch(), with the specific field(s) that
 *      diverged called out explicitly (not just the first one) to help
 *      narrow down which format handler is at fault.
 *
 * The *first* reported mismatch in a run is the one to actually chase -
 * once state has diverged, every mismatch after that is just noise,
 * not an independent bug. Capped at MAX_JIT_MISMATCH_COUNT total reports
 * per run to avoid runaway log growth once something is actually broken.
 ***************************************************************************/

#include "Debug.h"
#include "JITDifferential.h"

#ifdef JIT_DIFFERENTIAL_TESTING

#include "JIT.h"
#include "GBA.h"
#include "GBAcpu.h"
#include "GBAinline.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define JIT_DIFFERENTIAL_MAX_CATCHUP 14
#define JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN 8

extern u8 smcPageFlags[65536];

struct MemoryWriteEntry {
    u32 address;
    u32 value;
    u32 oldValue;
    u8 size; // 1 = byte, 2 = halfword, 4 = word
};

struct CPUStateBackup {
    u32 regs[16];
    CPUFlags flags;
    u32 armNextPC;
    u32 totalTicks;
    u32 prefetch[2];
    u32 busPrefetchCount;
    bool armState;
    bool stopState;
    bool holdState;
    int holdType;
    int cpuNextEvent;
};

// Tracks individual instruction execution & side effects during catch-up
struct CatchupTrace {
    u32 pc;
    u16 opcode;
    int cycles;
    int accumulatedCycles;
    CPUStateBackup stateAfter;
    u8 writeCount;
    MemoryWriteEntry writes[JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN];
};

// Hook state for intercepting C++ memory writes during catch-up
static bool g_diffTrackingActive = false;
static bool g_catchupSMCHit = false;
static u8 g_currentInsnWriteCount = 0;
static MemoryWriteEntry g_currentInsnWrites[JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN];

// --- TEMPORARY: R5 trace buffer (see JITDifferential.h) ---
u32 g_jitR5Trace[JIT_R5_TRACE_MAX];
u32 g_jitR5TraceTags[JIT_R5_TRACE_MAX];
u32 g_jitR5TraceIndex = 0;
u32 g_jitR5DumpSpill[4];

void JIT_ResetR5Trace() {
    g_jitR5TraceIndex = 0;
}

// Safely grabs original values without acknowledging hardware events (timers, DMA, etc.)
static u32 JIT_ReadMemoryRaw(u32 address, u8 size) {
    u8 pageIdx = address >> 24;
    u8* base = nullptr;
    u32 mask = 0;

    switch (pageIdx) {
        case 2: base = workRAM; mask = 0x3FFFF; break;
        case 3: base = internalRAM; mask = 0x7FFF; break;
        case 4: base = ioMem; mask = 0x3FF; break;
        case 5: base = paletteRAM; mask = 0x3FF; break;
        case 6: base = vram; mask = 0x1FFFF; break;
        case 7: base = oam; mask = 0x3FF; break;
        case 8: case 9: case 10: case 11: case 12: base = rom; mask = 0x1FFFFFF; break;
    }

    if (base != nullptr) {
        u32 addr = address & mask;
        if (size == 1) return base[addr];
        else if (size == 2) return base[addr] | (base[addr + 1] << 8);
        else return base[addr] | (base[addr + 1] << 8) | (base[addr + 2] << 16) | (base[addr + 3] << 24);
    }
    return 0;
}

// Safely rewinds memory writes without re-triggering emulator state logic backwards
static void JIT_RevertMemoryRaw(u32 address, u32 value, u8 size) {
    u8 pageIdx = address >> 24;
    u8* base = nullptr;
    u32 mask = 0;

    switch (pageIdx) {
        case 2: base = workRAM; mask = 0x3FFFF; break;
        case 3: base = internalRAM; mask = 0x7FFF; break;
        case 4: base = ioMem; mask = 0x3FF; break;
        case 5: base = paletteRAM; mask = 0x3FF; break;
        case 6: base = vram; mask = 0x1FFFF; break;
        case 7: base = oam; mask = 0x3FF; break;
    }

    if (base != nullptr) {
        u32 addr = address & mask;
        if (size == 1) {
            base[addr] = (u8)value;
        } else if (size == 2) {
            base[addr] = (u8)(value & 0xFF);
            base[addr + 1] = (u8)(value >> 8);
        } else {
            base[addr] = (u8)(value & 0xFF);
            base[addr + 1] = (u8)((value >> 8) & 0xFF);
            base[addr + 2] = (u8)((value >> 16) & 0xFF);
            base[addr + 3] = (u8)(value >> 24);
        }
    }
}

void JIT_RecordMemoryWrite(unsigned int addr, unsigned int value, unsigned char size) {
    if (!g_diffTrackingActive) return;

    // Detect if this specific instruction caused a JIT cache flush
    u8 pageIdx = addr >> 24;
    if (pageIdx == 2 || pageIdx == 3) {
        u32 page = (addr >> 10) & 0xFFFF;
        if (smcPageFlags[page]) {
            g_catchupSMCHit = true;
        }
    }

    if (g_currentInsnWriteCount < JIT_DIFFERENTIAL_MAX_WRITES_PER_INSN) {
        g_currentInsnWrites[g_currentInsnWriteCount].address = addr;
        g_currentInsnWrites[g_currentInsnWriteCount].value = value;
        g_currentInsnWrites[g_currentInsnWriteCount].size = size;

        // Fetch old state via direct read to avoid altering timing hardware state
        g_currentInsnWrites[g_currentInsnWriteCount].oldValue = JIT_ReadMemoryRaw(addr, size);

        g_currentInsnWriteCount++;
    }
}

static inline void JIT_SaveCPUState(CPUStateBackup* b) {
    for (int i = 0; i < 16; i++) b->regs[i] = reg[i].I;
    b->flags = gbaFlags;
    b->armNextPC = armNextPC;
    b->totalTicks = cpuTotalTicks;
    b->prefetch[0] = cpuPrefetch[0];
    b->prefetch[1] = cpuPrefetch[1];
    b->busPrefetchCount = busPrefetchCount;
    b->armState = armState;
    b->stopState = stopState;
    b->holdState = holdState;
    b->holdType = holdType;
    b->cpuNextEvent = cpuNextEvent;
}

static inline void JIT_RestoreCPUState(const CPUStateBackup* b) {
    for (int i = 0; i < 16; i++) reg[i].I = b->regs[i];
    gbaFlags = b->flags;
    armNextPC = b->armNextPC;
    cpuTotalTicks = b->totalTicks;
    cpuPrefetch[0] = b->prefetch[0];
    cpuPrefetch[1] = b->prefetch[1];
    busPrefetchCount = b->busPrefetchCount;
    armState = b->armState;
    stopState = b->stopState;
    holdState = b->holdState;
    holdType = b->holdType;
    cpuNextEvent = b->cpuNextEvent;
}

int JIT_RunDifferentialThumbHook_Impl(u32 pc, BasicBlock* block, u16 startOpcode, int* diffClockTicks, insnfunc_t* thumbInsnTable) {
    if (block == nullptr || block->execute == nullptr || debugStats.mismatchCount >= MAX_JIT_MISMATCH_COUNT || debugStats.isPCChecked(pc)) {
        return -1; // Proceed to normal JIT execution
    }

    debugStats.markPCChecked(pc);

    // 1. Save initial emulator state
    CPUStateBackup initial;
    JIT_SaveCPUState(&initial);

    // 2. Run Native C++ Catch-up execution (Interpreter) FIRST
    int cppCycles = 0;
    u32 instructionCount = 0;
    CatchupTrace catchupChain[JIT_DIFFERENTIAL_MAX_CATCHUP];

    // Enable write tracking hook
    g_diffTrackingActive = true;
    g_catchupSMCHit = false;

    u32 targetInstructions = block->length;
    if (targetInstructions > JIT_DIFFERENTIAL_MAX_CATCHUP) {
        targetInstructions = JIT_DIFFERENTIAL_MAX_CATCHUP;
    }

    // Execute ahead to chart the golden path
    while (instructionCount < targetInstructions && !armState && !holdState && !SWITicks) {
        u16 opcode = cpuPrefetch[0];
        cpuPrefetch[0] = cpuPrefetch[1];
        busPrefetch = false;

        if (busPrefetchCount & 0xFFFFFF00)
            busPrefetchCount = 0x100 | (busPrefetchCount & 0xFF);

        u32 oldArmNextPC = armNextPC;
        armNextPC = reg[15].I;
        reg[15].I += 2;

        THUMB_PREFETCH_NEXT;

        *diffClockTicks = 0;
        g_currentInsnWriteCount = 0;

        (*thumbInsnTable[opcode>>6])(opcode);

        if (*diffClockTicks < 0) {
            // Fatal C++ abort. Disable diff tracking and immediately bail.
            g_diffTrackingActive = false;
            return 0;
        }

        if (*diffClockTicks == 0) *diffClockTicks = codeTicksAccessSeq16(oldArmNextPC) + 1;

        cpuTotalTicks += *diffClockTicks;
        cppCycles += *diffClockTicks;

        // Snapshot execution details & writes performed by this instruction
        catchupChain[instructionCount].pc = oldArmNextPC;
        catchupChain[instructionCount].opcode = opcode;
        catchupChain[instructionCount].cycles = *diffClockTicks;
        catchupChain[instructionCount].accumulatedCycles = cppCycles;
        catchupChain[instructionCount].writeCount = g_currentInsnWriteCount;
        for (u8 w = 0; w < g_currentInsnWriteCount; w++) {
            catchupChain[instructionCount].writes[w] = g_currentInsnWrites[w];
        }

        // Save the exact CPU state after this instruction to act as the golden comparison point
        JIT_SaveCPUState(&catchupChain[instructionCount].stateAfter);

        instructionCount++;

        // --- NEW SAFETY BOUNDARIES ---

        // 1. Abort further block execution if we invalidated the JIT cache mid-stride
        if (g_catchupSMCHit) break;

        // 2. Abort further execution if the interpreter evaluated a branch
        if (armNextPC != oldArmNextPC + 2) break;

        // 3. Obey external hardware quotas
        if (cpuTotalTicks >= cpuNextEvent) break;
    }

    g_diffTrackingActive = false;

    // 3. Revert State and Memory Writes safely bypassing I/O triggers
    for (int i = instructionCount - 1; i >= 0; i--) {
        for (int w = catchupChain[i].writeCount - 1; w >= 0; w--) {
            const MemoryWriteEntry& write = catchupChain[i].writes[w];
            JIT_RevertMemoryRaw(write.address, write.oldValue, write.size);
        }
    }

    JIT_RestoreCPUState(&initial);

    // --- HANDLE SMC ABORT ---
    if (g_catchupSMCHit) {
        // We caught the C++ execution invalidating our block.
        // We've reverted our state; now formally return 3 to tell thumbExecute()
        // to disable JIT for this cycle and let the interpreter process the flush.
        return 3;
    }

    // 4. Run JIT Trace
    JITResult jitResult = {0, 0, 0, 0};
    reg[15].I = pc + 4;

	JIT_ResetR5Trace();
    ExecuteJITTrace(block->execute, &jitResult, &busPrefetchCount, &reg[0].I, &gbaFlags, &gbaReadTable);
    JIT_LOG_EXEC(jitResult.instructions, block->length, jitResult.bailedOut);

    // ALWAYS apply JIT state advancements to the global emulator state, leaving it exactly as JIT intended
    cpuTotalTicks += jitResult.cycles;

    if (jitResult.instructions > 0 || jitResult.bailedOut) {
        armNextPC = jitResult.nextPC;
        reg[15].I = armNextPC + 2;
        cpuPrefetch[0] = CPUReadHalfWord(armNextPC);
        cpuPrefetch[1] = CPUReadHalfWord(armNextPC + 2);
    }

    // Handle Bailouts and SMCs cleanly without generating comparison noise
    if (jitResult.smcHit || jitResult.bailedOut) {
        if (jitResult.smcHit) {
            jitCache.invalidateSMCTarget(jitResult.smcAddress);
        }
        return 3;
    }

    // Skip comparison if execution was totally out of bounds
    if (jitResult.instructions > JIT_DIFFERENTIAL_MAX_CATCHUP || jitResult.instructions == 0) {
        return 2;
    }

    // Skip comparison if the JIT executed more instructions than the C++ interpreter did (eg: block chaining)
    if(jitResult.instructions > instructionCount) {
    	return 2;
    }

    // 5. Compare & Detect Divergence
    CPUStateBackup jitState;
    JIT_SaveCPUState(&jitState);

    bool mismatch = false;
    bool instMismatch = false;
    CPUStateBackup cppState;
    int cppFinalCycles = 0;

    if (jitResult.instructions > instructionCount) {
        // The JIT plowed through an event boundary or instruction limit the C++ respected
        instMismatch = true;
        mismatch = true;
        // Fallback to the last available C++ state for logging
        u32 fallbackIndex = instructionCount > 0 ? instructionCount - 1 : 0;
        cppState = catchupChain[fallbackIndex].stateAfter;
        cppFinalCycles = catchupChain[fallbackIndex].accumulatedCycles;
    } else {
        // Index into the exact step C++ recorded to ensure apples-to-apples comparison
        u32 matchIndex = jitResult.instructions - 1;
        cppState = catchupChain[matchIndex].stateAfter;
        cppFinalCycles = catchupChain[matchIndex].accumulatedCycles;
    }

    bool regMismatches[15] = { false };
    bool pcMismatch = (jitResult.nextPC != cppState.armNextPC);
    bool flagMismatch = (jitState.flags.N != cppState.flags.N || jitState.flags.Z != cppState.flags.Z ||
                         jitState.flags.C != cppState.flags.C || jitState.flags.V != cppState.flags.V);
    bool cycleMismatch = ((int)jitResult.cycles != cppFinalCycles);
    bool prefetchMismatch = (jitState.busPrefetchCount != cppState.busPrefetchCount);

    for (int i = 0; i < 15; i++) {
        if (jitState.regs[i] != cppState.regs[i]) {
            regMismatches[i] = true;
            mismatch = true;
        }
    }

    if (cppState.armState && !initial.armState) {
        pcMismatch = false;
        cycleMismatch = false;
    }

    if (pcMismatch || flagMismatch || cycleMismatch || instMismatch || prefetchMismatch) mismatch = true;

    // Differential Testing Stats
    debugStats.diffTotalChecks++;

    int lengthBin = 0;
    if (jitResult.instructions <= 4) lengthBin = 0;
    else if (jitResult.instructions <= 8) lengthBin = 1;
    else if (jitResult.instructions <= 16) lengthBin = 2;
    else if (jitResult.instructions <= 32) lengthBin = 3;
    else if (jitResult.instructions <= 64) lengthBin = 4;
    else lengthBin = 5;

    if (mismatch) {
        debugStats.diffMismatches++;
        debugStats.diffMismatchOpcodeFreq[startOpcode >> 6]++;
        debugStats.diffMismatchLengthBins[lengthBin]++;

        if (instMismatch) debugStats.diffMismatchInst++;
        if (pcMismatch) debugStats.diffMismatchPC++;
        if (cycleMismatch) debugStats.diffMismatchCycles++;
        if (prefetchMismatch) debugStats.diffMismatchPrefetch++;

        if (flagMismatch) {
            debugStats.diffMismatchFlags++;
            if (jitState.flags.N != cppState.flags.N) debugStats.diffMismatchFlagSpecific[0]++;
            if (jitState.flags.Z != cppState.flags.Z) debugStats.diffMismatchFlagSpecific[1]++;
            if (jitState.flags.C != cppState.flags.C) debugStats.diffMismatchFlagSpecific[2]++;
            if (jitState.flags.V != cppState.flags.V) debugStats.diffMismatchFlagSpecific[3]++;
        }

        for (int i = 0; i < 15; i++) {
            if (regMismatches[i]) {
                debugStats.diffMismatchRegs++;
                debugStats.diffMismatchRegSpecific[i]++;
            }
        }

        // Deep Opcode Profiling: Mark executed instructions as Suspects
        for (u32 i = 0; i < jitResult.instructions && i < instructionCount; i++) {
            debugStats.deepOpcodeSuspectFreq[catchupChain[i].opcode >> 6]++;
        }
    } else {
        debugStats.diffMatches++;
        debugStats.diffMatchOpcodeFreq[startOpcode >> 6]++;

        // Deep Opcode Profiling: Mark executed instructions as Successes
        for (u32 i = 0; i < jitResult.instructions; i++) {
            debugStats.deepOpcodeSuccessFreq[catchupChain[i].opcode >> 6]++;
        }
    }

    if(mismatch) {
		debugStats.mismatchCount++;
	}

    // 6. Log Detailed Mismatch State
    if (mismatch && debugStats.mismatchCount <= MAX_JIT_MISMATCH_DETAILED_COUNT) {
        static const char* regNames[15] = {
            "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
            "R8", "R9", "R10", "R11", "R12", "SP", "LR"
        };

        static char assembledMsg[8192];
        char *ptr = assembledMsg;
        size_t remaining = sizeof(assembledMsg);
        assembledMsg[0] = '\0';

        auto appendToMsg = [&](const char* format, ...) {
            va_list args;
            va_start(args, format);
            int written = vsnprintf(ptr, remaining, format, args);
            va_end(args);

            if (written > 0) {
                if ((size_t)written >= remaining) {
                    ptr += remaining - 1;
                    remaining = 1;
                } else {
                    ptr += written;
                    remaining -= (size_t)written;
                }
            }
        };

        appendToMsg("StartPC: 0x%08X | Trace Length: %u | Opcode: 0x%04X\n", pc, instructionCount, startOpcode);
        appendToMsg("Initial Flags: N=%d Z=%d C=%d V=%d\n", initial.flags.N, initial.flags.Z, initial.flags.C, initial.flags.V);
        appendToMsg("Entry busPrefetchCount: 0x%08X\n", initial.busPrefetchCount);
        appendToMsg("JIT Result:  NextPC=0x%08X | Cycles=%u | Flags=(N:%u Z:%u C:%u V:%u) | Insns=%u\n",
            jitResult.nextPC, jitResult.cycles, jitState.flags.N, jitState.flags.Z, jitState.flags.C, jitState.flags.V, jitResult.instructions);
        appendToMsg("C++ Result:  NextPC=0x%08X | Cycles=%d | Flags=(N:%u Z:%u C:%u V:%u) | Insns=%u\n",
            cppState.armNextPC, cppFinalCycles, cppState.flags.N, cppState.flags.Z, cppState.flags.C, cppState.flags.V, instructionCount);

        appendToMsg("--- MISMATCH DETAILS ---\n");
        if (instMismatch) {
            appendToMsg("  [INSTRUCTIONS] JIT claimed %u vs C++ ran %u\n", jitResult.instructions, instructionCount);
        }
        if (prefetchMismatch) {
            appendToMsg("  [PREFETCH] JIT=0x%08X vs C++=0x%08X\n", jitState.busPrefetchCount, cppState.busPrefetchCount);
        }
        for (int i = 0; i < 15; i++) {
            if (regMismatches[i]) {
                appendToMsg("  [REG %-3s] Initial=0x%08X | JIT=0x%08X vs C++=0x%08X\n",
                    regNames[i], initial.regs[i], jitState.regs[i], cppState.regs[i]);
            }
        }
        if (pcMismatch) {
            appendToMsg("  [NEXT PC] JIT=0x%08X vs C++=0x%08X\n", jitResult.nextPC, cppState.armNextPC);
        }
        if (flagMismatch) {
            appendToMsg("  [FLAGS]   JIT=(N:%u Z:%u C:%u V:%u) vs C++=(N:%u Z:%u C:%u V:%u)\n",
                jitState.flags.N, jitState.flags.Z, jitState.flags.C, jitState.flags.V,
                cppState.flags.N, cppState.flags.Z, cppState.flags.C, cppState.flags.V);
        }
        if (cycleMismatch) {
            appendToMsg("  [CYCLES]  JIT=%u vs C++=%d\n", jitResult.cycles, cppFinalCycles);
        }
        if (cppState.armState && !initial.armState) {
            appendToMsg("  [NOTE] Interpreter entered ARM mode during catch-up...\n");
        }

        appendToMsg("\n--- C++ EXECUTION TRACE (Ran %u insns) ---\n", instructionCount);
        for (u32 i = 0; i < instructionCount; i++) {
            const CatchupTrace& tr = catchupChain[i];
            appendToMsg("  [%02u] PC: 0x%08X | Opcode: 0x%04X | Cycles: %d | Writes: %u | busPrefetchCount(after)=0x%08X\n",
            	i, tr.pc, tr.opcode, tr.cycles, tr.writeCount, tr.stateAfter.busPrefetchCount);

            for (u8 w = 0; w < tr.writeCount; w++) {
                const MemoryWriteEntry& write = tr.writes[w];
                appendToMsg("       -> [WRITE %2ubit] Addr: 0x%08X | NewVal: 0x%08X | OldVal: 0x%08X\n",
                    write.size * 8, write.address, write.value, write.oldValue);
            }
        }
        appendToMsg("\n--- JIT R5 (live busPrefetchCount) TRACE (%u entries) ---\n", g_jitR5TraceIndex);
		{
        	static const char* tagNames[9] = {
        	    "?",
        	    "PUSH/POP-before    ",
        	    "PUSH/POP-after     ",
        	    "Branch-before      ",
        	    "Branch-after       ",
        	    "Trace-entry        ",
        	    "SingleAccess-entry ",
        	    "SingleAccess-mid   ",
        	    "SingleAccess-after "
        	};
        	u32 n = g_jitR5TraceIndex < JIT_R5_TRACE_MAX ? g_jitR5TraceIndex : JIT_R5_TRACE_MAX;
        	for (u32 i = 0; i < n; i++) {
        	    u32 tag   = g_jitR5TraceTags[i] >> 24;
        	    u32 tagPC = g_jitR5TraceTags[i] & 0xFFFFFF;
        	    const char* name = (tag < 9) ? tagNames[tag] : "?";
        	    appendToMsg("  [%02u] %s PC:0x%06X R5=0x%08X\n", i, name, tagPC, g_jitR5Trace[i]);
        	}
		}

		JIT_LOG_MISMATCH(assembledMsg);
    }

    // Check if the freshly left-in-place JIT state tripped an event boundary
    if (cpuTotalTicks >= cpuNextEvent || armState || holdState || SWITicks) return 1;

    return 2;
}

#endif // JIT_DIFFERENTIAL_TESTING

#ifndef JIT_DIFFERENTIAL_H
#define JIT_DIFFERENTIAL_H

#ifdef JIT_DIFFERENTIAL_TESTING

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

struct CPUStateBackup {
    u32 regs[16];
    CPUFlags flags;
    u32 armNextPC;
    u32 totalTicks;
    u32 prefetch[2];
    u32 busPrefetchCount;
};

inline void JIT_SaveCPUState(CPUStateBackup* b, u32* regI, CPUFlags flags, u32 armNextPC, u32 totalTicks, u32* prefetch, u32 busPrefetchCount) {
    for (int i = 0; i < 16; i++) b->regs[i] = regI[i];
    b->flags = flags;
    b->armNextPC = armNextPC;
    b->totalTicks = totalTicks;
    b->prefetch[0] = prefetch[0];
    b->prefetch[1] = prefetch[1];
    b->busPrefetchCount = busPrefetchCount;
}

inline void JIT_RestoreCPUState(const CPUStateBackup* b, u32* regI, CPUFlags* flags, u32* armNextPC, u32* totalTicks, u32* prefetch, u32* busPrefetchCount) {
    for (int i = 0; i < 16; i++) regI[i] = b->regs[i];
    *flags = b->flags;
    *armNextPC = b->armNextPC;
    *totalTicks = b->totalTicks;
    prefetch[0] = b->prefetch[0];
    prefetch[1] = b->prefetch[1];
    *busPrefetchCount = b->busPrefetchCount;
}

template<typename StepFunc>
inline int JIT_RunDifferentialThumbHook(
    u32 pc,
    BasicBlock* block,
    u32* regI,
    CPUFlags* gbaFlags,
    u32* armNextPC,
    u32* cpuTotalTicks,
    u32* cpuPrefetch,
    u32* busPrefetchCount,
    u32 cpuNextEvent,
    bool* armState,
    bool* holdState,
    bool* SWITicks,
    u16 startOpcode,
    StepFunc stepCppFallback)
{
    if (block == nullptr || block->execute == nullptr || jitStats.mismatchCount >= MAX_JIT_MISMATCH_COUNT) {
        return 0; // Did not handle, proceed to normal fallback
    }

    // 1. Save state
    CPUStateBackup initial;
    JIT_SaveCPUState(&initial, regI, *gbaFlags, *armNextPC, *cpuTotalTicks, cpuPrefetch, *busPrefetchCount);

    // 2. Run JIT
    JITResult jitResult = {0, 0, 0, 0};
    regI[15] = pc + 4;
    ExecuteJITTrace(block->execute, &jitResult, busPrefetchCount, &regI[0], gbaFlags, &gbaReadTable);

    JIT_LOG_EXEC(jitResult.instructions, block->length, jitResult.bailedOut);

    // 3. Save JIT output & restore initial
    CPUStateBackup jitState;
    JIT_SaveCPUState(&jitState, regI, *gbaFlags, *armNextPC, *cpuTotalTicks, cpuPrefetch, *busPrefetchCount);
    JIT_RestoreCPUState(&initial, regI, gbaFlags, armNextPC, cpuTotalTicks, cpuPrefetch, busPrefetchCount);

    // 4. Run C++ Catch-up
    int cppCycles = 0;
    const u32 kMaxSteps = 8192;
    u32 steps = 0;
    int cppLocalTicks = 0;
    bool preventDeadlock = jitResult.instructions == 0;

    while (preventDeadlock || (cppCycles < (int)jitResult.cycles && steps < kMaxSteps &&
           *cpuTotalTicks < cpuNextEvent && !(*armState) && !(*holdState) && !(*SWITicks))) {
        preventDeadlock = false;
        cppLocalTicks = stepCppFallback();

        if (cppLocalTicks < 0) break;

        cppCycles += cppLocalTicks;
        steps++;
    }

    // 5. Compare & Detect
    bool armModeDuringCatchup = *armState;
    bool mismatch = false;
    bool regMismatches[15] = { false };
    bool pcMismatch = (jitResult.nextPC != *armNextPC);
    bool flagMismatch = (jitState.flags.N != gbaFlags->N || jitState.flags.Z != gbaFlags->Z ||
                         jitState.flags.C != gbaFlags->C || jitState.flags.V != gbaFlags->V);
    bool cycleMismatch = ((int)jitResult.cycles != cppCycles);

    for (int i = 0; i < 15; i++) {
        if (jitState.regs[i] != regI[i]) {
            regMismatches[i] = true;
            mismatch = true;
        }
    }

    if (pcMismatch || flagMismatch || cycleMismatch) mismatch = true;

    // 6. Log Detailed Mismatch
    if (mismatch) {
        static const char* regNames[15] = {
            "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
            "R8", "R9", "R10", "R11", "R12", "SP", "LR"
        };

        static char assembledMsg[4096];
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

        appendToMsg("StartPC: 0x%08X | Trace Length: %u | Opcode: 0x%04X\n", pc, jitResult.instructions, startOpcode);
        appendToMsg("Initial Flags: N=%u Z=%u C=%u V=%u\n", initial.flags.N, initial.flags.Z, initial.flags.C, initial.flags.V);
        appendToMsg("JIT Result:  NextPC=0x%08X | Cycles=%u | Flags=(N:%u Z:%u C:%u V:%u)\n",
            jitResult.nextPC, jitResult.cycles, jitState.flags.N, jitState.flags.Z, jitState.flags.C, jitState.flags.V);
        appendToMsg("C++ Result:  NextPC=0x%08X | Cycles=%d | Flags=(N:%u Z:%u C:%u V:%u)\n",
            *armNextPC, cppCycles, gbaFlags->N, gbaFlags->Z, gbaFlags->C, gbaFlags->V);

        appendToMsg("--- MISMATCH DETAILS ---\n");
        for (int i = 0; i < 15; i++) {
            if (regMismatches[i]) {
                appendToMsg("  [REG %-3s] Initial=0x%08X | JIT=0x%08X vs C++=0x%08X\n",
                    regNames[i], initial.regs[i], jitState.regs[i], regI[i]);
            }
        }
        if (pcMismatch) {
            appendToMsg("  [NEXT PC] JIT=0x%08X vs C++=0x%08X\n", jitResult.nextPC, *armNextPC);
        }
        if (flagMismatch) {
            appendToMsg("  [FLAGS]   JIT=(N:%u Z:%u C:%u V:%u) vs C++=(N:%u Z:%u C:%u V:%u)\n",
                jitState.flags.N, jitState.flags.Z, jitState.flags.C, jitState.flags.V,
                gbaFlags->N, gbaFlags->Z, gbaFlags->C, gbaFlags->V);
        }
        if (cycleMismatch) {
            appendToMsg("  [CYCLES]  JIT=%u vs C++=%d\n", jitResult.cycles, cppCycles);
        }
        if (armModeDuringCatchup) {
            appendToMsg("  [NOTE] Interpreter entered ARM mode during catch-up - this comparison reflects a shadow-harness blind spot (Thumb-only catch-up can't follow a real mode switch), not a JIT bug. The JIT itself bails cleanly at the BX in this case.\n");
        }

        JIT_LOG_MISMATCH(assembledMsg);
    }

    if (cppLocalTicks < 0) return -1;
    if (*cpuTotalTicks >= cpuNextEvent || *armState || *holdState || *SWITicks) return 1;
    return 2; // Success/Handled, continue loop
}

#define JIT_DIFFERENTIAL_THUMB_HOOK(pc, block) \
    do { \
        int diffState = JIT_RunDifferentialThumbHook( \
            (pc), (block), &reg[0].I, &gbaFlags, &armNextPC, &cpuTotalTicks, \
            cpuPrefetch, &busPrefetchCount, cpuNextEvent, &armState, &holdState, &SWITicks, \
            CPUReadHalfWord(pc), \
            [&]() -> int { \
                u32 opcode = cpuPrefetch[0]; \
                cpuPrefetch[0] = cpuPrefetch[1]; \
                busPrefetch = false; \
                if (busPrefetchCount & 0xFFFFFF00) busPrefetchCount = 0x100 | (busPrefetchCount & 0xFF); \
                u32 oldArmNextPC = armNextPC; \
                armNextPC = reg[15].I; \
                reg[15].I += 2; \
                THUMB_PREFETCH_NEXT; \
                clockTicks = 0; \
                (*thumbInsnTable[opcode>>6])(opcode); \
                if (clockTicks < 0) return clockTicks; \
                if (clockTicks == 0) clockTicks = codeTicksAccessSeq16(oldArmNextPC) + 1; \
                cpuTotalTicks += clockTicks; \
                return clockTicks; \
            } \
        ); \
        if (diffState == -1) { PROFILER_ADD_TIME(timeSpentThumb, thumbTimeStart); return 0; } \
        if (diffState == 1) { PROFILER_ADD_TIME(timeSpentThumb, thumbTimeStart); return 1; } \
        if (diffState == 2) continue; \
    } while(0)
#endif // JIT_DIFFERENTIAL_TESTING

#endif // JIT_DIFFERENTIAL_H

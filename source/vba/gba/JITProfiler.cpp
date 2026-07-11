#include "JITProfiler.h"
#include "JITDebugLog.h"
#include <stdio.h>
#include <algorithm>
#include <ogc/system.h>

JITStats g_jitStats;

void JITStats::reset() {
    jitInstructionsExecuted = 0;
    fallbackInstructionsExecuted = 0;
    blocksCompiled = 0;
    for (int i = 0; i < 1024; i++) {
        fallbackOpcodeFreq[i] = 0;
        compileBailoutFreq[i] = 0;
    }
    for (int i = 0; i < BAILOUT_REASON_COUNT; i++) {
        bailoutReasons[i] = 0;
    }
}

void JITStats::print() {
    u64 total = jitInstructionsExecuted + fallbackInstructionsExecuted;
    if (total == 0) return;

    double jitPct = (double)jitInstructionsExecuted / total * 100.0;

    LOG("\n========== JIT PROFILING STATS ==========\n");
    LOG("Total Instructions: %llu\n", total);
    LOG("JIT Handled:        %llu (%.2f%%)\n", jitInstructionsExecuted, jitPct);
    LOG("Fallback (Interp):  %llu (%.2f%%)\n", fallbackInstructionsExecuted, 100.0 - jitPct);
    LOG("Blocks Compiled:    %u\n", blocksCompiled);
    LOG("-----------------------------------------\n");

    LOG("Bailout Reasons:\n");
    LOG("  Unsupported opcode:    %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_OPCODE]);
    LOG("  Buffer Overflow:       %u\n", bailoutReasons[BAILOUT_BUFFER_OVERFLOW]);
    LOG("  Unsupported ALU:       %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_ALU]);
    LOG("  Unsupported Mem Bank:  %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_MEM_BANK]);
    LOG("  ARM Mode Switch:       %u\n", bailoutReasons[BAILOUT_ARM_SWITCH]);
    LOG("  Conditional Branch:    %u\n", bailoutReasons[BAILOUT_CONDITIONAL_BRANCH]);
    LOG("  Branch with Link:      %u\n", bailoutReasons[BAILOUT_BRANCH_WITH_LINK]);
    LOG("  Instr Count Zero:      %u\n", bailoutReasons[BAILOUT_INSTR_COUNT_ZERO]);
    LOG("  Unknown MEM Op:        %u\n", bailoutReasons[BAILOUT_UNKNOWN_MEM_OP]);
    LOG("  No Push/Pop Regs:      %u\n", bailoutReasons[BAILOUT_PUSH_POP_REGS]);
    LOG("  No LDMIA/STMIA Regs:   %u\n", bailoutReasons[BAILOUT_LDMIA_STMIA_REGS]);
    LOG("-----------------------------------------\n");

    // Top 10 Fallback Executions
    struct Stat { u16 bucket; u64 count; };
    Stat topFallback[1024];
    for (int i = 0; i < 1024; i++) {
        topFallback[i].bucket = i;
        topFallback[i].count = fallbackOpcodeFreq[i];
    }

    // Sort descending by count
    std::sort(topFallback, topFallback + 1024, [](const Stat& a, const Stat& b) {
        return a.count > b.count;
    });

    LOG("Top 10 Fallback Executions (Interpreter):\n");
    for (int i = 0; i < 10; i++) {
        if (topFallback[i].count == 0)
            continue;
        LOG("  #%d: Opcode Prefix ~0x%04X (Bucket %4d) - %llu times\n",
               i + 1, topFallback[i].bucket << 6, topFallback[i].bucket, topFallback[i].count);
    }
    LOG("=========================================\n\n");

    WriteJITLogToFile();
}

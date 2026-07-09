#include "JITProfiler.h"
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

    SYS_Report("\n========== JIT PROFILING STATS ==========\n");
    SYS_Report("Total Instructions: %llu\n", total);
    SYS_Report("JIT Handled:        %llu (%.2f%%)\n", jitInstructionsExecuted, jitPct);
    SYS_Report("Fallback (Interp):  %llu (%.2f%%)\n", fallbackInstructionsExecuted, 100.0 - jitPct);
    SYS_Report("Blocks Compiled:    %u\n", blocksCompiled);
    SYS_Report("-----------------------------------------\n");

    SYS_Report("Bailout Reasons:\n");
    SYS_Report("  Buffer Overflow:       %u\n", bailoutReasons[BAILOUT_BUFFER_OVERFLOW]);
    SYS_Report("  Unsupported ALU:       %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_ALU]);
    SYS_Report("  Composite Flags:       %u\n", bailoutReasons[BAILOUT_COMPOSITE_FLAGS]);
    SYS_Report("  Unsupported Mem Bank:  %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_MEM_BANK]);
    SYS_Report("  ARM Mode Switch:       %u\n", bailoutReasons[BAILOUT_ARM_SWITCH]);
    SYS_Report("-----------------------------------------\n");

    // Fix for the empty Top 10 list:
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

    SYS_Report("Top 10 Fallback Executions (Interpreter):\n");
    for (int i = 0; i < 10; i++) {
        if (topFallback[i].count == 0)
        	continue;
        SYS_Report("  #%d: Opcode Prefix ~0x%04X (Bucket %4d) - %llu times\n",
               i + 1, topFallback[i].bucket << 6, topFallback[i].bucket, topFallback[i].count);
    }
    SYS_Report("=========================================\n\n");
}

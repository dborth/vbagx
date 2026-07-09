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

    char buf[2048];
    size_t offset = 0;

    // Helper lambda to append formatted strings into the single buffer
    auto append = [&](const char* fmt, ...) {
        if (offset >= sizeof(buf)) return;
        va_list args;
        va_start(args, fmt);
        int written = vsnprintf(buf + offset, sizeof(buf) - offset, fmt, args);
        va_end(args);
        if (written > 0) {
            offset += written;
            if (offset > sizeof(buf)) offset = sizeof(buf);
        }
    };

    append("\n========== JIT PROFILING STATS ==========\n");
    append("Total Instructions: %llu\n", total);
    append("JIT Handled:        %llu (%.2f%%)\n", jitInstructionsExecuted, jitPct);
    append("Fallback (Interp):  %llu (%.2f%%)\n", fallbackInstructionsExecuted, 100.0 - jitPct);
    append("Blocks Compiled:    %u\n", blocksCompiled);
    append("-----------------------------------------\n");

    append("Bailout Reasons:\n");
    append("  Buffer Overflow:       %u\n", bailoutReasons[BAILOUT_BUFFER_OVERFLOW]);
    append("  Unsupported ALU:       %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_ALU]);
    append("  Composite Flags:       %u\n", bailoutReasons[BAILOUT_COMPOSITE_FLAGS]);
    append("  Unsupported Mem Bank:  %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_MEM_BANK]);
    append("  ARM Mode Switch:       %u\n", bailoutReasons[BAILOUT_ARM_SWITCH]);
    append("  Conditional Branch:    %u\n", bailoutReasons[BAILOUT_CONDITIONAL_BRANCH]);
    append("  MEM Instr Count 0:     %u\n", bailoutReasons[BAILOUT_MEM_INSTR_COUNT_ZERO]);
    append("  MEM:                   %u\n", bailoutReasons[BAILOUT_MEM]);
    append("-----------------------------------------\n");

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

    append("Top 10 Fallback Executions (Interpreter):\n");
    for (int i = 0; i < 10; i++) {
        if (topFallback[i].count == 0)
            continue;
        append("  #%d: Opcode Prefix ~0x%04X (Bucket %4d) - %llu times\n",
               i + 1, topFallback[i].bucket << 6, topFallback[i].bucket, topFallback[i].count);
    }
    append("=========================================\n\n");

    // Single call to print the assembled message
    SYS_Report("%s", buf);
}

#include "JITProfiler.h"
#include <stdio.h>
#include <algorithm>
#include <ogc/system.h>
#include "JITDebug.h"

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
    mismatchCount = 0;
    traceLogCount = 0;
}

void JITStats::print() {
    u64 total = jitInstructionsExecuted + fallbackInstructionsExecuted;

    double jitPct = (double)jitInstructionsExecuted / total * 100.0;

    JIT_LOG("\n========== JIT PROFILING STATS ==========\n");
    JIT_LOG("Total Instructions: %llu\n", total);
    JIT_LOG("JIT Handled:        %llu (%.2f%%)\n", jitInstructionsExecuted, jitPct);
    JIT_LOG("Fallback (Interp):  %llu (%.2f%%)\n", fallbackInstructionsExecuted, 100.0 - jitPct);
    JIT_LOG("Blocks Compiled:    %u\n", blocksCompiled);
    JIT_LOG("-----------------------------------------\n");

    JIT_LOG("Bailout Reasons:\n");
    JIT_LOG("  Unsupported opcode:     			%u\n", bailoutReasons[BAILOUT_UNSUPPORTED_OPCODE]);
    JIT_LOG("  Buffer Overflow:        			%u\n", bailoutReasons[BAILOUT_BUFFER_OVERFLOW]);
    JIT_LOG("  Unsupported ALU:        			%u\n", bailoutReasons[BAILOUT_UNSUPPORTED_ALU]);
    JIT_LOG("  Unsupported Mem Bank:   			%u\n", bailoutReasons[BAILOUT_UNSUPPORTED_MEM_BANK]);
    JIT_LOG("  ARM Mode Switch:        			%u\n", bailoutReasons[BAILOUT_ARM_SWITCH]);
    JIT_LOG("  Conditional Branch:     			%u\n", bailoutReasons[BAILOUT_CONDITIONAL_BRANCH]);
    JIT_LOG("  Branch with Link: 		      	%u\n", bailoutReasons[BAILOUT_BRANCH_WITH_LINK]);
    JIT_LOG("  TMB9/10/11 Instr Count Zero:		%u\n", bailoutReasons[BAILOUT_TMB9_10_11_INSTR_COUNT_ZERO]);
    JIT_LOG("  THB14 Instr Count Zero: 			%u\n", bailoutReasons[BAILOUT_THB14_INSTR_COUNT_ZERO]);
    JIT_LOG("  THB15 Instr Count Zero: 			%u\n", bailoutReasons[BAILOUT_THB15_INSTR_COUNT_ZERO]);
    JIT_LOG("  Unknown MEM Op:         			%u\n", bailoutReasons[BAILOUT_UNKNOWN_MEM_OP]);
    JIT_LOG("  No Push/Pop Regs:       			%u\n", bailoutReasons[BAILOUT_PUSH_POP_REGS]);
    JIT_LOG("  No LDMIA/STMIA Regs:    			%u\n", bailoutReasons[BAILOUT_LDMIA_STMIA_REGS]);
    JIT_LOG("  High-Register CMP:     			%u\n", bailoutReasons[BAILOUT_HIGH_REGISTER_CMP]);
    JIT_LOG("-----------------------------------------\n");

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

    JIT_LOG("Top 10 Fallback Executions (Interpreter):\n");
    for (int i = 0; i < 10; i++) {
        if (topFallback[i].count == 0)
            continue;
        JIT_LOG("  #%d: Opcode Prefix ~0x%04X (Bucket %4d) - %llu times\n",
               i + 1, topFallback[i].bucket << 6, topFallback[i].bucket, topFallback[i].count);
    }
    JIT_LOG("=========================================\n\n");
    WriteJITLogToFile();
}

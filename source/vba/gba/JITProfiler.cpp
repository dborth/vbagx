#ifndef NO_JIT_COMPILER
#include "JITProfiler.h"
#include <stdio.h>
#include <algorithm>
#include <ogc/system.h>
#include "JITDebug.h"

JITStats jitStats;

void JITStats::reset() {
    jitInstructionsExecuted = 0;
    fallbackInstructionsExecuted = 0;
    blocksCompiled = 0;

    timeSpentCompiling = 0;
	timeSpentJIT = 0;
	timeSpentFallback = 0;
	jitInvocations = 0;
	fallbackInvocations = 0;
	for (int i = 0; i < 6; i++) blockLengthBins[i] = 0;

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
	u64 totalInstr = jitInstructionsExecuted + fallbackInstructionsExecuted;
	double jitPct = (double)jitInstructionsExecuted / totalInstr * 100.0;

	u64 totalTime = timeSpentCompiling + timeSpentJIT + timeSpentFallback;
	double timeCompilePct = totalTime > 0 ? ((double)timeSpentCompiling / totalTime * 100.0) : 0.0;
	double timeJitPct = totalTime > 0 ? ((double)timeSpentJIT / totalTime * 100.0) : 0.0;
	double timeFallbackPct = totalTime > 0 ? ((double)timeSpentFallback / totalTime * 100.0) : 0.0;

	double avgBlockSize = jitInvocations > 0 ? ((double)jitInstructionsExecuted / jitInvocations) : 0.0;
	double compilePerBlock = blocksCompiled > 0 ? ((double)timeSpentCompiling / blocksCompiled) : 0.0;

	JIT_LOG("\n========== JIT PROFILING STATS ==========\n");

	JIT_LOG("--- TIME DISTRIBUTION ---\n");
	JIT_LOG("Time in Compiler: %.2f%%\n", timeCompilePct);
	JIT_LOG("Time in JIT Exec: %.2f%%\n", timeJitPct);
	JIT_LOG("Time in Fallback: %.2f%%\n", timeFallbackPct);
	JIT_LOG("Avg Ticks to Compile 1 Block: %.2f\n", compilePerBlock);
	JIT_LOG("-----------------------------------------\n");

	JIT_LOG("--- EXECUTION & HOPPING ---\n");
	JIT_LOG("JIT Invocations (Hops in): %llu\n", jitInvocations);
	JIT_LOG("Fallback Invocations:      %llu\n", fallbackInvocations);
	JIT_LOG("Total Instructions:        %llu\n", totalInstr);
	JIT_LOG("JIT Handled:               %llu (%.2f%%)\n", jitInstructionsExecuted, jitPct);
	JIT_LOG("Fallback (Interp):         %llu (%.2f%%)\n", fallbackInstructionsExecuted, 100.0 - jitPct);
	JIT_LOG("Average JIT Block Size:    %.2f instructions\n", avgBlockSize);
	JIT_LOG("-----------------------------------------\n");

	JIT_LOG("--- BLOCK SIZE DISTRIBUTION ---\n");
	JIT_LOG("Blocks Compiled: %u\n", blocksCompiled);
	JIT_LOG("  1 to 4   Insns: %u\n", blockLengthBins[0]);
	JIT_LOG("  5 to 8   Insns: %u\n", blockLengthBins[1]);
	JIT_LOG("  9 to 16  Insns: %u\n", blockLengthBins[2]);
	JIT_LOG(" 17 to 32  Insns: %u\n", blockLengthBins[3]);
	JIT_LOG(" 33 to 64  Insns: %u\n", blockLengthBins[4]);
	JIT_LOG(" 65+       Insns: %u\n", blockLengthBins[5]);
	JIT_LOG("-----------------------------------------\n");

	JIT_LOG("Bailout Reasons:\n");
	JIT_LOG("  Unsupported opcode:               %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_OPCODE]);
	JIT_LOG("  Buffer Overflow:                  %u\n", bailoutReasons[BAILOUT_BUFFER_OVERFLOW]);
	JIT_LOG("  Unsupported ALU:                  %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_ALU]);
	JIT_LOG("  Unsupported Mem Bank:             %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_MEM_BANK]);
	JIT_LOG("  ARM Mode Switch:                  %u\n", bailoutReasons[BAILOUT_ARM_SWITCH]);
	JIT_LOG("  Conditional Branch:               %u\n", bailoutReasons[BAILOUT_CONDITIONAL_BRANCH]);
	JIT_LOG("  Branch with Link:                 %u\n", bailoutReasons[BAILOUT_BRANCH_WITH_LINK]);
	JIT_LOG("  TMB9/10/11 Instr Count Zero:      %u\n", bailoutReasons[BAILOUT_TMB9_10_11_INSTR_COUNT_ZERO]);
	JIT_LOG("  THB14 Instr Count Zero:           %u\n", bailoutReasons[BAILOUT_THB14_INSTR_COUNT_ZERO]);
	JIT_LOG("  THB15 Instr Count Zero:           %u\n", bailoutReasons[BAILOUT_THB15_INSTR_COUNT_ZERO]);
	JIT_LOG("  Unknown MEM Op:                   %u\n", bailoutReasons[BAILOUT_UNKNOWN_MEM_OP]);
	JIT_LOG("  No Push/Pop Regs:                 %u\n", bailoutReasons[BAILOUT_PUSH_POP_REGS]);
	JIT_LOG("  No LDMIA/STMIA Regs:              %u\n", bailoutReasons[BAILOUT_LDMIA_STMIA_REGS]);
	JIT_LOG("-----------------------------------------\n");

	// Top 10 Fallback Executions
	struct Stat { u16 bucket; u64 count; };
	Stat topFallback[1024];
	for (int i = 0; i < 1024; i++) {
		topFallback[i].bucket = i;
		topFallback[i].count = fallbackOpcodeFreq[i];
	}

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
#endif

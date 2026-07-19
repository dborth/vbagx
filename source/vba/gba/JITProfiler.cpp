#ifndef NO_JIT_COMPILER
#include <stdio.h>
#include <algorithm>
#include <ogc/system.h>
#include <ogc/lwp_watchdog.h>
#include "JITCache.h"

JITStats jitStats;

void JITStats::reset() {
    timeTotalStart = gettime(); // Automatically drops anchor on JIT_RESET_LOGS()
    timeTotalElapsed = 0;
    timeSpentThumb = 0;
    timeSpentARM = 0;

    timeSpentCompiling = 0;
	timeSpentJIT = 0;
	timeSpentFallback = 0;

    jitInstructionsExecuted = 0;
    fallbackInstructionsExecuted = 0;
    blocksCompiled = 0;
	jitInvocations = 0;
	fallbackInvocations = 0;

	for (int i = 0; i < 6; i++) blockLengthBins[i] = 0;
    for (int i = 0; i < 1024; i++) {
        fallbackOpcodeFreq[i] = 0;
        compileBailoutFreq[i] = 0;
    }
    for (int i = 0; i < BAILOUT_REASON_COUNT; i++) bailoutReasons[i] = 0;

    mismatchCount = 0;
    traceLogCount = 0;
}

void JITStats::print() {
    // 1. Calculate Real-World Seconds
    timeTotalElapsed = gettime() - timeTotalStart;

    double totalSecs   = ticks_to_microsecs(timeTotalElapsed) / 1000000.0;
    double thumbSecs   = ticks_to_microsecs(timeSpentThumb) / 1000000.0;
    double armSecs     = ticks_to_microsecs(timeSpentARM) / 1000000.0;
    double compileSecs = ticks_to_microsecs(timeSpentCompiling) / 1000000.0;
    double jitSecs     = ticks_to_microsecs(timeSpentJIT) / 1000000.0;
    double fallSecs    = ticks_to_microsecs(timeSpentFallback) / 1000000.0;
    double otherSecs   = totalSecs - (thumbSecs + armSecs);

    // 2. Calculate Percentages
    double thumbPct  = totalSecs > 0 ? (thumbSecs / totalSecs * 100.0) : 0.0;
    double armPct    = totalSecs > 0 ? (armSecs / totalSecs * 100.0) : 0.0;
    double otherPct  = totalSecs > 0 ? (otherSecs / totalSecs * 100.0) : 0.0;

    double compPct   = thumbSecs > 0 ? (compileSecs / thumbSecs * 100.0) : 0.0;
    double execPct   = thumbSecs > 0 ? (jitSecs / thumbSecs * 100.0) : 0.0;
    double interpPct = thumbSecs > 0 ? (fallSecs / thumbSecs * 100.0) : 0.0;

    // 3. Print the Hierarchical Time Breakdown
	JIT_LOG("\n========== JIT REAL-TIME PROFILING ==========\n");
	JIT_LOG("Total Wall-Clock Time: %.3f seconds\n\n", totalSecs);

	JIT_LOG("THUMB Execution: %.3f seconds (%.1f%% of Total)\n", thumbSecs, thumbPct);
	JIT_LOG("  Compiling JIT: %.3f seconds (%.1f%% of THUMB)\n", compileSecs, compPct);
	JIT_LOG("  Executing JIT: %.3f seconds (%.1f%% of THUMB)\n", jitSecs, execPct);
	JIT_LOG("  Interpreter:   %.3f seconds (%.1f%% of THUMB)\n", fallSecs, interpPct);

	JIT_LOG("\nARM Execution:   %.3f seconds (%.1f%% of Total)\n", armSecs, armPct);
	JIT_LOG("Other / Core:    %.3f seconds (%.1f%% of Total)\n", otherSecs, otherPct);
	JIT_LOG("---------------------------------------------\n");

    // 4. Print Instruction & Hop Data
	u64 totalInstr = jitInstructionsExecuted + fallbackInstructionsExecuted;
	double jitInstrPct = totalInstr > 0 ? ((double)jitInstructionsExecuted / totalInstr * 100.0) : 0.0;
	double avgBlockSize = jitInvocations > 0 ? ((double)jitInstructionsExecuted / jitInvocations) : 0.0;

	double jitIPS  = jitSecs > 0  ? ((double)jitInstructionsExecuted / jitSecs) : 0.0;
	double fallIPS = fallSecs > 0 ? ((double)fallbackInstructionsExecuted / fallSecs) : 0.0;

	JIT_LOG("--- EXECUTION & HOPPING ---\n");
	JIT_LOG("Total Instructions: %llu\n", totalInstr);
	JIT_LOG("JIT Handled:        %llu (%.2f%%)\n", jitInstructionsExecuted, jitInstrPct);
	JIT_LOG("Fallback (Interp):  %llu (%.2f%%)\n", fallbackInstructionsExecuted, 100.0 - jitInstrPct);
	JIT_LOG("JIT Hops In:        %llu\n", jitInvocations);
	JIT_LOG("Fallback Hops:      %llu\n", fallbackInvocations);
	JIT_LOG("Avg JIT Block Size: %.2f instructions\n", avgBlockSize);
	JIT_LOG("JIT Exec Speed:     %.0f instr/sec (%.2f MIPS)\n", jitIPS, jitIPS / 1000000.0);
	JIT_LOG("Interpreter Speed:  %.0f instr/sec (%.2f MIPS)\n", fallIPS, fallIPS / 1000000.0);
	JIT_LOG("-----------------------------------------\n");

    // 5. Print Block Distribution
	JIT_LOG("--- BLOCK SIZE DISTRIBUTION ---\n");
	JIT_LOG("Blocks Compiled: %u\n", blocksCompiled);
	JIT_LOG("  1 to 4   Insns: %u\n", blockLengthBins[0]);
	JIT_LOG("  5 to 8   Insns: %u\n", blockLengthBins[1]);
	JIT_LOG("  9 to 16  Insns: %u\n", blockLengthBins[2]);
	JIT_LOG(" 17 to 32  Insns: %u\n", blockLengthBins[3]);
	JIT_LOG(" 33 to 64  Insns: %u\n", blockLengthBins[4]);
	JIT_LOG(" 65+       Insns: %u\n", blockLengthBins[5]);
	JIT_LOG("-----------------------------------------\n");

    // 6. Print Bailouts
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

	// 7. Top 10 Fallbacks
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

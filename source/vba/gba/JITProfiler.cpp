#ifndef NO_JIT_COMPILER

#if JIT_PROFILING
#include <stdio.h>
#include <algorithm>
#include "JIT.h"

JITStats jitStats;

void JITStats::reset() {
    timeTotalStart = gettime(); // Automatically drops anchor on JIT_RESET_LOGS()
    timeTotalElapsed = 0;
    timeSpentThumb = 0;
    timeSpentARM = 0;

    timeSpentCompiling = 0;
	timeSpentJIT = 0;
	timeSpentFallback = 0;
	timeSpentFlushing = 0;

    jitInstructionsExecuted = 0;
    fallbackInstructionsExecuted = 0;
    blocksCompiled = 0;
    blacklistedBlocks = 0;

	cacheFlushes = 0;
	cacheHits = 0;
	cacheMisses = 0;
	cacheEvictions = 0;

    thumbInvocations = 0;
	armInvocations = 0;
	swiInvocations = 0;
	jitInvocations = 0;
	fallbackInvocations = 0;

	for (int i = 0; i < 6; i++) blockLengthBins[i] = 0;
    for (int i = 0; i < 1024; i++) {
        fallbackOpcodeFreq[i] = 0;
        compileBailoutFreq[i] = 0;
    }
    for (int i = 0; i < BAILOUT_REASON_COUNT; i++) bailoutReasons[i] = 0;

    fullBlockCompletions = 0;
	partialBlockExecutions = 0;
	bailoutToJitTransitions = 0;
	for (int i = 0; i < 5; i++) blockExecutionRatioBins[i] = 0;
	for (int i = 0; i < 6; i++) bailoutOffsetBins[i] = 0;

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
    double flushSecs   = ticks_to_microsecs(timeSpentFlushing) / 1000000.0;
    double otherSecs   = totalSecs - (thumbSecs + armSecs);

    // Calculate Invocations Per Second rates
    double avgFPS = totalSecs > 0 ? ((double)framesRendered / totalSecs) : 0.0;
	double thumbInvPerSec = totalSecs > 0 ? ((double)thumbInvocations / totalSecs) : 0.0;
	double armInvPerSec   = totalSecs > 0 ? ((double)armInvocations / totalSecs) : 0.0;
	double swiInvPerSec   = totalSecs > 0 ? ((double)swiInvocations / totalSecs) : 0.0;

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
	JIT_LOG("Average Framerate:     %.2f FPS\n\n", avgFPS);

	JIT_LOG("--- MODE INVOCATIONS PER SECOND ---\n");
	JIT_LOG("THUMB Invocations:  %llu (~%.2f/sec)\n", thumbInvocations, thumbInvPerSec);
	JIT_LOG("ARM Invocations:    %llu (~%.2f/sec)\n", armInvocations, armInvPerSec);
	JIT_LOG("SWI Invocations:    %llu (~%.2f/sec)\n", swiInvocations, swiInvPerSec);

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
	JIT_LOG("--- BLOCK DISTRIBUTION ---\n");
	JIT_LOG("Blacklisted: %u\n", blacklistedBlocks);
	JIT_LOG("Blocks Compiled: %u\n", blocksCompiled);
	JIT_LOG("  1 to 4   Insns: %u\n", blockLengthBins[0]);
	JIT_LOG("  5 to 8   Insns: %u\n", blockLengthBins[1]);
	JIT_LOG("  9 to 16  Insns: %u\n", blockLengthBins[2]);
	JIT_LOG(" 17 to 32  Insns: %u\n", blockLengthBins[3]);
	JIT_LOG(" 33 to 64  Insns: %u\n", blockLengthBins[4]);
	JIT_LOG(" 65+       Insns: %u\n", blockLengthBins[5]);
	JIT_LOG("-----------------------------------------\n");

	// 6. Print Cache Statistics
	JIT_LOG("--- CACHE STATISTICS ---\n");
	JIT_LOG("Cache Flushes:      %u\n", cacheFlushes);
	JIT_LOG("Time Flushing:      %.3f seconds\n", flushSecs);
	JIT_LOG("Cache Hits:         %u\n", cacheHits);
	JIT_LOG("Cache Misses:       %u\n", cacheMisses);
	JIT_LOG("Cache Evictions:    %u\n", cacheEvictions);
	u32 totalLookups = cacheHits + cacheMisses;
	double hitRate = totalLookups > 0 ? ((double)cacheHits / totalLookups * 100.0) : 0.0;
	JIT_LOG("Hit Rate:           %.2f%%\n", hitRate);
	JIT_LOG("-----------------------------------------\n");

    // 7. Print Bailouts
	JIT_LOG("Bailout Reasons:\n");
	JIT_LOG("  Unsupported opcode:               %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_OPCODE]);
	JIT_LOG("  Unsupported FMT 14 opcode:        %u\n", bailoutReasons[BAILOUT_FMT14_UNSUPPORTED_OPCODE]);
	JIT_LOG("  Buffer Overflow:                  %u\n", bailoutReasons[BAILOUT_BUFFER_OVERFLOW]);
	JIT_LOG("  SWI opcode:                       %u\n", bailoutReasons[BAILOUT_SWI_OPCODE]);
	JIT_LOG("  Conditional Branch:               %u\n", bailoutReasons[BAILOUT_CONDITIONAL_BRANCH]);
	JIT_LOG("  Branch with Link:                 %u\n", bailoutReasons[BAILOUT_BRANCH_WITH_LINK]);
	JIT_LOG("  No Push/Pop Regs:                 %u\n", bailoutReasons[BAILOUT_PUSH_POP_REGS]);
	JIT_LOG("  No LDMIA/STMIA Regs:              %u\n", bailoutReasons[BAILOUT_LDMIA_STMIA_REGS]);
	JIT_LOG("  Unsupported Mem Bank:             %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_MEM_BANK]);
	JIT_LOG("-----------------------------------------\n");
#if JIT_BLOCK_FRAGMENTATION_STATS
	JIT_LOG("--- JIT BLOCK LIFECYCLE & FRAGMENTATION STATS ---\n");	
	u32 totalExecs = fullBlockCompletions + partialBlockExecutions;
	double fullPct = totalExecs > 0 ? ((double)fullBlockCompletions / totalExecs * 100.0) : 0.0;
	double partPct = totalExecs > 0 ? ((double)partialBlockExecutions / totalExecs * 100.0) : 0.0;

	JIT_LOG("Block Execution Completions: %u (%.1f%%)\n", fullBlockCompletions, fullPct);
	JIT_LOG("Mid-Block Bailouts (Partial): %u (%.1f%%)\n", partialBlockExecutions, partPct);
	JIT_LOG("Bailout-to-JIT Transitions:  %u\n", bailoutToJitTransitions);

	JIT_LOG("\nExecution Coverage Ratio:\n");
	JIT_LOG(" [0%% - 25%%]   Executed: %u\n", blockExecutionRatioBins[0]);
	JIT_LOG(" [25%% - 50%%]  Executed: %u\n", blockExecutionRatioBins[1]);
	JIT_LOG(" [50%% - 75%%]  Executed: %u\n", blockExecutionRatioBins[2]);
	JIT_LOG(" [75%% - 100%%] Executed: %u\n", blockExecutionRatioBins[3]);
	JIT_LOG(" [100%%]                : %u\n", blockExecutionRatioBins[4]);

	JIT_LOG("\nBailout Offset Distribution (Instructions Executed):\n");
	JIT_LOG("  Inst 0 (Immediate): %u\n", bailoutOffsetBins[0]);
	JIT_LOG("  Inst 1-3:           %u\n", bailoutOffsetBins[1]);
	JIT_LOG("  Inst 4-7:           %u\n", bailoutOffsetBins[2]);
	JIT_LOG("  Inst 8-15:          %u\n", bailoutOffsetBins[3]);
	JIT_LOG("  Inst 16-31:         %u\n", bailoutOffsetBins[4]);
	JIT_LOG("  Inst 32+:           %u\n", bailoutOffsetBins[5]);
	JIT_LOG("----------------------------------------------------------\n");
#endif
	// 8. Top 10 Fallbacks
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
#endif

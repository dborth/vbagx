#if VBAGX_DEBUG
#include <stdio.h>
#include <algorithm>
#include "JIT.h"

DebugStats debugStats;

void DebugStats::reset() {
	timeTotalStart = gettime(); // Automatically drops anchor on DEBUG_RESET_LOGS()
	timeTotalElapsed = 0;

	minCoreFps = 999.0f; maxCoreFps = 0.0f; accumCoreFps = 0.0;
	minRenderFps = 999.0f; maxRenderFps = 0.0f; accumRenderFps = 0.0;
	fpsSamples = 0;
	for (int i = 0; i < 5; i++) { coreFpsBins[i] = 0; renderFpsBins[i] = 0; }

	audioStarvationEvents = 0;
	audioOverflowDrops = 0;
	for (int i = 0; i < 13; i++) audioBufferFullnessBins[i] = 0;
	for (int i = 0; i < 3; i++) drcStateTicks[i] = 0;
	drcTransitions = 0;
	currentDrcState = 0; // RATE_STATE_NEUTRAL

	framesSkippedTotal = 0;
	consecutiveSkips = 0;
	for (int i = 0; i < 6; i++) consecutiveFrameskipBins[i] = 0;

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

	diffTotalChecks = 0;
	diffMatches = 0;
	diffMismatches = 0;
	diffMismatchInst = 0;
	diffMismatchPC = 0;
	diffMismatchFlags = 0;
	diffMismatchCycles = 0;
	diffMismatchPrefetch = 0;
	diffMismatchRegs = 0;
	for (int i = 0; i < 1024; i++) {
		diffMatchOpcodeFreq[i] = 0;
		diffMismatchOpcodeFreq[i] = 0;
	}
    mismatchCount = 0;
    traceLogCount = 0;
}

void DebugStats::print() {
    timeTotalElapsed = gettime() - timeTotalStart;
    double totalSecs   = ticks_to_microsecs(timeTotalElapsed) / 1000000.0;

	DEBUG_LOG("Total Wall-Clock Time: %.3f seconds\n\n", totalSecs);

	DEBUG_LOG("--- PERFORMANCE & TIMING TUNING ---\n");
	float avgCore = fpsSamples > 0 ? (float)(accumCoreFps / fpsSamples) : 0.0f;
	float avgDisp = fpsSamples > 0 ? (float)(accumRenderFps / fpsSamples) : 0.0f;

	DEBUG_LOG("Core FPS:    Min: %5.1f | Max: %5.1f | Avg: %5.1f\n", minCoreFps, maxCoreFps, avgCore);
	DEBUG_LOG("Render FPS: Min: %5.1f | Max: %5.1f | Avg: %5.1f\n", minRenderFps, minRenderFps, avgDisp);
	DEBUG_LOG("FPS Histogram (<50 | 50-55 | 55-59 | 59-61 | >61):\n");
	DEBUG_LOG("  Core:    [%4u | %4u | %4u | %4u | %4u]\n", coreFpsBins[0], coreFpsBins[1], coreFpsBins[2], coreFpsBins[3], coreFpsBins[4]);
	DEBUG_LOG("  Display: [%4u | %4u | %4u | %4u | %4u]\n", renderFpsBins[0], renderFpsBins[1], renderFpsBins[2], renderFpsBins[3], renderFpsBins[4]);

	DEBUG_LOG("\nFrameskip Health (Micro-stutter Analysis):\n");
	DEBUG_LOG("  Total Frames Skipped: %u\n", framesSkippedTotal);
	DEBUG_LOG("  Consecutive Skips Histogram (1, 2, 3, 4, 5, 6+):\n");
	DEBUG_LOG("  [%4u | %4u | %4u | %4u | %4u | %4u]\n",
		consecutiveFrameskipBins[0], consecutiveFrameskipBins[1], consecutiveFrameskipBins[2],
		consecutiveFrameskipBins[3], consecutiveFrameskipBins[4], consecutiveFrameskipBins[5]);

	DEBUG_LOG("\nAudio Buffer & DRC Health:\n");
	DEBUG_LOG("  Absolute Starvation Events (Audio Dropouts): %u\n", audioStarvationEvents);
	DEBUG_LOG("  Overflow Drops (DMA ring full, chunk discarded): %u\n", audioOverflowDrops);
	DEBUG_LOG("  DRC State Ticks - Neutral: %u | Draining: %u | Filling: %u\n", drcStateTicks[0], drcStateTicks[1], drcStateTicks[2]);
	DEBUG_LOG("  DRC State Transitions: %u\n", drcTransitions);
	DEBUG_LOG("  Buffer Fullness Histogram (Target is 4-8):\n");
	for (int i = 0; i < 13; i++) {
		DEBUG_LOG("    [%2d] Buffers: %u\n", i, audioBufferFullnessBins[i]);
	}
	DEBUG_LOG("-----------------------------------------\n");

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
	DEBUG_LOG("\n========== JIT REAL-TIME PROFILING ==========\n");
	DEBUG_LOG("Average Framerate:     %.2f FPS\n\n", avgFPS);

	DEBUG_LOG("--- MODE INVOCATIONS PER SECOND ---\n");
	DEBUG_LOG("THUMB Invocations:  %llu (~%.2f/sec)\n", thumbInvocations, thumbInvPerSec);
	DEBUG_LOG("ARM Invocations:    %llu (~%.2f/sec)\n", armInvocations, armInvPerSec);
	DEBUG_LOG("SWI Invocations:    %llu (~%.2f/sec)\n", swiInvocations, swiInvPerSec);

	DEBUG_LOG("THUMB Execution: %.3f seconds (%.1f%% of Total)\n", thumbSecs, thumbPct);
	DEBUG_LOG("  Compiling JIT: %.3f seconds (%.1f%% of THUMB)\n", compileSecs, compPct);
	DEBUG_LOG("  Executing JIT: %.3f seconds (%.1f%% of THUMB)\n", jitSecs, execPct);
	DEBUG_LOG("  Interpreter:   %.3f seconds (%.1f%% of THUMB)\n", fallSecs, interpPct);

	DEBUG_LOG("\nARM Execution:   %.3f seconds (%.1f%% of Total)\n", armSecs, armPct);
	DEBUG_LOG("Other / Core:    %.3f seconds (%.1f%% of Total)\n", otherSecs, otherPct);
	DEBUG_LOG("---------------------------------------------\n");

    // 4. Print Instruction & Hop Data
	u64 totalInstr = jitInstructionsExecuted + fallbackInstructionsExecuted;
	double jitInstrPct = totalInstr > 0 ? ((double)jitInstructionsExecuted / totalInstr * 100.0) : 0.0;
	double avgBlockSize = jitInvocations > 0 ? ((double)jitInstructionsExecuted / jitInvocations) : 0.0;

	double jitIPS  = jitSecs > 0  ? ((double)jitInstructionsExecuted / jitSecs) : 0.0;
	double fallIPS = fallSecs > 0 ? ((double)fallbackInstructionsExecuted / fallSecs) : 0.0;

	DEBUG_LOG("--- EXECUTION & HOPPING ---\n");
	DEBUG_LOG("Total Instructions: %llu\n", totalInstr);
	DEBUG_LOG("JIT Handled:        %llu (%.2f%%)\n", jitInstructionsExecuted, jitInstrPct);
	DEBUG_LOG("Fallback (Interp):  %llu (%.2f%%)\n", fallbackInstructionsExecuted, 100.0 - jitInstrPct);
	DEBUG_LOG("JIT Hops In:        %llu\n", jitInvocations);
	DEBUG_LOG("Fallback Hops:      %llu\n", fallbackInvocations);
	DEBUG_LOG("Avg JIT Block Size: %.2f instructions\n", avgBlockSize);
	DEBUG_LOG("JIT Exec Speed:     %.0f instr/sec (%.2f MIPS)\n", jitIPS, jitIPS / 1000000.0);
	DEBUG_LOG("Interpreter Speed:  %.0f instr/sec (%.2f MIPS)\n", fallIPS, fallIPS / 1000000.0);
	DEBUG_LOG("-----------------------------------------\n");

    // 5. Print Block Distribution
	DEBUG_LOG("--- BLOCK DISTRIBUTION ---\n");
	DEBUG_LOG("Blacklisted: %u\n", blacklistedBlocks);
	DEBUG_LOG("Blocks Compiled: %u\n", blocksCompiled);
	DEBUG_LOG("  1 to 4   Insns: %u\n", blockLengthBins[0]);
	DEBUG_LOG("  5 to 8   Insns: %u\n", blockLengthBins[1]);
	DEBUG_LOG("  9 to 16  Insns: %u\n", blockLengthBins[2]);
	DEBUG_LOG(" 17 to 32  Insns: %u\n", blockLengthBins[3]);
	DEBUG_LOG(" 33 to 64  Insns: %u\n", blockLengthBins[4]);
	DEBUG_LOG(" 65+       Insns: %u\n", blockLengthBins[5]);
	DEBUG_LOG("-----------------------------------------\n");

	// 6. Print Cache Statistics
	DEBUG_LOG("--- CACHE STATISTICS ---\n");
	DEBUG_LOG("Cache Flushes:      %u\n", cacheFlushes);
	DEBUG_LOG("Time Flushing:      %.3f seconds\n", flushSecs);
	DEBUG_LOG("Cache Hits:         %u\n", cacheHits);
	DEBUG_LOG("Cache Misses:       %u\n", cacheMisses);
	DEBUG_LOG("Cache Evictions:    %u\n", cacheEvictions);
	u32 totalLookups = cacheHits + cacheMisses;
	double hitRate = totalLookups > 0 ? ((double)cacheHits / totalLookups * 100.0) : 0.0;
	DEBUG_LOG("Hit Rate:           %.2f%%\n", hitRate);
	DEBUG_LOG("-----------------------------------------\n");

    // 7. Print Bailouts
	DEBUG_LOG("Bailout Reasons:\n");
	DEBUG_LOG("  Unsupported opcode:               %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_OPCODE]);
	DEBUG_LOG("  Unsupported FMT 14 opcode:        %u\n", bailoutReasons[BAILOUT_FMT14_UNSUPPORTED_OPCODE]);
	DEBUG_LOG("  Buffer Overflow:                  %u\n", bailoutReasons[BAILOUT_BUFFER_OVERFLOW]);
	DEBUG_LOG("  SWI opcode:                       %u\n", bailoutReasons[BAILOUT_SWI_OPCODE]);
	DEBUG_LOG("  Conditional Branch:               %u\n", bailoutReasons[BAILOUT_CONDITIONAL_BRANCH]);
	DEBUG_LOG("  Branch with Link:                 %u\n", bailoutReasons[BAILOUT_BRANCH_WITH_LINK]);
	DEBUG_LOG("  No Push/Pop Regs:                 %u\n", bailoutReasons[BAILOUT_PUSH_POP_REGS]);
	DEBUG_LOG("  No LDMIA/STMIA Regs:              %u\n", bailoutReasons[BAILOUT_LDMIA_STMIA_REGS]);
	DEBUG_LOG("  Unsupported Mem Bank:             %u\n", bailoutReasons[BAILOUT_UNSUPPORTED_MEM_BANK]);
	DEBUG_LOG("-----------------------------------------\n");
#if JIT_BLOCK_FRAGMENTATION_STATS
	DEBUG_LOG("--- JIT BLOCK LIFECYCLE & FRAGMENTATION STATS ---\n");	
	u32 totalExecs = fullBlockCompletions + partialBlockExecutions;
	double fullPct = totalExecs > 0 ? ((double)fullBlockCompletions / totalExecs * 100.0) : 0.0;
	double partPct = totalExecs > 0 ? ((double)partialBlockExecutions / totalExecs * 100.0) : 0.0;

	DEBUG_LOG("Block Execution Completions: %u (%.1f%%)\n", fullBlockCompletions, fullPct);
	DEBUG_LOG("Mid-Block Bailouts (Partial): %u (%.1f%%)\n", partialBlockExecutions, partPct);
	DEBUG_LOG("Bailout-to-JIT Transitions:  %u\n", bailoutToJitTransitions);

	DEBUG_LOG("\nExecution Coverage Ratio:\n");
	DEBUG_LOG(" [0%% - 25%%]   Executed: %u\n", blockExecutionRatioBins[0]);
	DEBUG_LOG(" [25%% - 50%%]  Executed: %u\n", blockExecutionRatioBins[1]);
	DEBUG_LOG(" [50%% - 75%%]  Executed: %u\n", blockExecutionRatioBins[2]);
	DEBUG_LOG(" [75%% - 100%%] Executed: %u\n", blockExecutionRatioBins[3]);
	DEBUG_LOG(" [100%%]                : %u\n", blockExecutionRatioBins[4]);

	DEBUG_LOG("\nBailout Offset Distribution (Instructions Executed):\n");
	DEBUG_LOG("  Inst 0 (Immediate): %u\n", bailoutOffsetBins[0]);
	DEBUG_LOG("  Inst 1-3:           %u\n", bailoutOffsetBins[1]);
	DEBUG_LOG("  Inst 4-7:           %u\n", bailoutOffsetBins[2]);
	DEBUG_LOG("  Inst 8-15:          %u\n", bailoutOffsetBins[3]);
	DEBUG_LOG("  Inst 16-31:         %u\n", bailoutOffsetBins[4]);
	DEBUG_LOG("  Inst 32+:           %u\n", bailoutOffsetBins[5]);
	DEBUG_LOG("----------------------------------------------------------\n");
#endif

#ifdef JIT_DIFFERENTIAL_TESTING
	DEBUG_LOG("--- DIFFERENTIAL TESTING STATS ---\n");
	DEBUG_LOG("Total Execution Checks: %u\n", diffTotalChecks);

	if (diffTotalChecks > 0) {
		double matchPct = ((double)diffMatches / diffTotalChecks) * 100.0;
		double missPct = ((double)diffMismatches / diffTotalChecks) * 100.0;
		DEBUG_LOG("Perfect Matches: %u (%.2f%%)\n", diffMatches, matchPct);
		DEBUG_LOG("Mismatches:      %u (%.2f%%)\n", diffMismatches, missPct);
	}

	if (diffMismatches > 0) {
		DEBUG_LOG("\nMismatch Root-Cause Distribution (Can Overlap):\n");
		DEBUG_LOG("  Registers Diverged:    %u (cumulative reg misses)\n", diffMismatchRegs);
		DEBUG_LOG("  Flags Diverged:        %u\n", diffMismatchFlags);
		DEBUG_LOG("  Timing/Cycles Drift:   %u\n", diffMismatchCycles);
		DEBUG_LOG("  Next PC Diverged:      %u\n", diffMismatchPC);
		DEBUG_LOG("  Instruction Count:     %u\n", diffMismatchInst);
		DEBUG_LOG("  Prefetch Buffer Drift: %u\n", diffMismatchPrefetch);

		// Top 10 Mismatching Opcodes & their failure rates
		struct DiffStat { u16 bucket; u32 mismatches; u32 matches; };
		DiffStat topDiff[1024];
		for (int i = 0; i < 1024; i++) {
			topDiff[i].bucket = i;
			topDiff[i].mismatches = diffMismatchOpcodeFreq[i];
			topDiff[i].matches = diffMatchOpcodeFreq[i];
		}

		std::sort(topDiff, topDiff + 1024, [](const DiffStat& a, const DiffStat& b) {
			return a.mismatches > b.mismatches;
		});

		DEBUG_LOG("\nTop 10 Mismatching Opcode Groups:\n");
		for (int i = 0; i < 10; i++) {
			if (topDiff[i].mismatches == 0) break;
			u32 totalHits = topDiff[i].mismatches + topDiff[i].matches;
			double failRate = ((double)topDiff[i].mismatches / totalHits) * 100.0;

			DEBUG_LOG("  #%d: Opcode Prefix ~0x%04X (Bucket %4d) | Mismatches: %u | Fail Rate: %5.1f%%\n",
				i + 1, topDiff[i].bucket << 6, topDiff[i].bucket, topDiff[i].mismatches, failRate);
		}
	}
	DEBUG_LOG("----------------------------------------------------------\n");
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

	DEBUG_LOG("Top 10 Fallback Executions (Interpreter):\n");
	for (int i = 0; i < 10; i++) {
		if (topFallback[i].count == 0)
			continue;
		DEBUG_LOG("  #%d: Opcode Prefix ~0x%04X (Bucket %4d) - %llu times\n",
			   i + 1, topFallback[i].bucket << 6, topFallback[i].bucket, topFallback[i].count);
	}
	DEBUG_LOG("=========================================\n\n");
	WriteDebugLogToFile();
}

void DebugStats::recordFPS(float coreFPS, float renderFPS) {
	if (coreFPS < minCoreFps && coreFPS > 0.0f) minCoreFps = coreFPS;
	if (coreFPS > maxCoreFps) maxCoreFps = coreFPS;
	accumCoreFps += coreFPS;

	if (renderFPS < minRenderFps && renderFPS > 0.0f) minRenderFps = renderFPS;
	if (renderFPS > maxRenderFps) maxRenderFps = renderFPS;
	accumRenderFps += renderFPS;

	fpsSamples++;

	if (coreFPS < 50.0f) coreFpsBins[0]++;
	else if (coreFPS < 55.0f) coreFpsBins[1]++;
	else if (coreFPS < 59.0f) coreFpsBins[2]++;
	else if (coreFPS <= 61.0f) coreFpsBins[3]++;
	else coreFpsBins[4]++;

	if (renderFPS < 50.0f) renderFpsBins[0]++;
	else if (renderFPS < 55.0f) renderFpsBins[1]++;
	else if (renderFPS < 59.0f) renderFpsBins[2]++;
	else if (renderFPS <= 61.0f) renderFpsBins[3]++;
	else renderFpsBins[4]++;
}

void DebugStats::commitFrameskip() {
	if (consecutiveSkips == 0) return;
	if (consecutiveSkips <= 5) consecutiveFrameskipBins[consecutiveSkips - 1]++;
	else consecutiveFrameskipBins[5]++;
	consecutiveSkips = 0; // Reset for the next run
}

void DebugStats::updateDRC(int unplayed, int newState) {
	if (unplayed <= 12) audioBufferFullnessBins[unplayed]++;

	drcStateTicks[newState]++;
	if (currentDrcState != newState) {
		drcTransitions++;
		currentDrcState = newState;
	}
}
#endif

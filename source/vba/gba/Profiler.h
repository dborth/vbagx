#ifndef PROFILER_H
#define PROFILER_H

#if VBAGX_DEBUG
#include "../common/Port.h"

static const int MAX_JIT_TRACE_CALLS = 100;
static const int MAX_JIT_MISMATCH_COUNT = 500;
static const int MAX_JIT_MISMATCH_DETAILED_COUNT = 30;
#define DIFF_PC_HASH_SIZE 4096

enum BailoutReason {
	BAILOUT_UNKNOWN = 0,
	BAILOUT_UNSUPPORTED_OPCODE,
	BAILOUT_FMT14_UNSUPPORTED_OPCODE,
	BAILOUT_BUFFER_OVERFLOW,
	BAILOUT_UNSUPPORTED_MEM_BANK,
	BAILOUT_SWI_OPCODE,
	BAILOUT_CONDITIONAL_BRANCH,
	BAILOUT_BRANCH_WITH_LINK,
	BAILOUT_PUSH_POP_REGS,
	BAILOUT_LDMIA_STMIA_REGS,
	BAILOUT_REASON_COUNT
};

struct DebugStats {
    u64 timeTotalStart;
    u64 timeTotalElapsed;

	// Framerate Stats (Core vs Render)
	float minCoreFps, maxCoreFps, minRenderFps, maxRenderFps;
	double accumCoreFps, accumRenderFps;
	u32 fpsSamples;
	u32 coreFpsBins[5];    // <50, 50-55, 55-59, 59-61, >61
	u32 renderFpsBins[5]; // <50, 50-55, 55-59, 59-61, >61

	// Audio & DRC Stats
	u32 audioStarvationEvents;
	u32 audioOverflowDrops; // DMA ring was full; a chunk was decoded then discarded
	u32 audioBufferFullnessBins[13];
	u32 drcStateTicks[3]; // 0: Neutral, 1: Draining, 2: Filling
	u32 drcTransitions;
	int currentDrcState;  // Internal tracker to avoid polluting OgcEmulatorAudio.cpp

	// Frameskip Stats
	u32 framesSkippedTotal;
	u32 consecutiveSkips; // Tracked directly via PROFILER_INC
	u32 consecutiveFrameskipBins[6]; // 1, 2, 3, 4, 5, 6+ skips

    u64 timeSpentThumb;
    u64 timeSpentARM;

	u64 timeSpentCompiling;
	u64 timeSpentJIT;
	u64 timeSpentFallback;
	u64 timeSpentFlushing;

	u64 jitInstructionsExecuted;
	u64 fallbackInstructionsExecuted;
	u32 blocksCompiled;
	u32 blacklistedBlocks;
	u32 blockLengthBins[6];

	u32 cacheFlushes;
	u32 cacheHits;
	u32 cacheMisses;
	u32 cacheEvictions;

	// Wii U codegen RW-/R-X toggle cost (WutCodegenBeginWrite/EndWrite)
	u64 timeSpentCodegenToggle;
	u32 codegenToggleCount;
	u32 codegenScopesCompile; // JITWriteScope opened by JITCompileThumbTrace
	u32 codegenScopesFlush;   // ...by JITCache::flushCache
	u32 codegenScopesSMC;     // ...by JITCache::invalidateSMCTarget

	// Self-modifying-code guard traffic. A "call" is one
	// invalidateSMCTarget() invocation; "patched" means it actually found
	// and evicted >=1 overlapping compiled block (as opposed to firing
	// on a page flag with no real block collision on this specific EA).
	u32 smcInvalidateCalls;
	u32 smcInvalidateFromJIT;   // triggered by a JIT-emitted SMC guard bailout
	u32 smcInvalidateFromWrite; // triggered by CPUWrite*() in the interpreter
	u32 smcInvalidatePatched;

	// JIT quota-shield yields (256-cycle trace budget hit mid-block).
	// Heuristically identified in the dispatch loop (bailedOut &&
	// !smcHit && instructions==0 && nextPC==entryPC) rather than a
	// dedicated JITResult flag, so treat as approximate.
	u32 quotaYields;

	u64 thumbInvocations;
	u64 armInvocations;
	u64 swiInvocations;

	u64 jitInvocations;
	u64 fallbackInvocations;

	u64 fallbackOpcodeFreq[1024];
	u32 compileBailoutFreq[1024];
	u32 bailoutReasons[BAILOUT_REASON_COUNT];

	u32 fullBlockCompletions;
	u32 partialBlockExecutions;
	u32 blockExecutionRatioBins[5];
	u32 midBlockRecompilations;
	u32 bailoutOffsetBins[6];
	u32 bailoutToJitTransitions;

	u32 diffTotalChecks;
	u32 diffMatches;
	u32 diffMismatches;
	u32 diffMatchOpcodeFreq[1024];
	u32 diffMismatchOpcodeFreq[1024];

	u32 diffMismatchInst;
	u32 diffMismatchPC;
	u32 diffMismatchFlags;
	u32 diffMismatchCycles;
	u32 diffMismatchPrefetch;
	u32 diffMismatchRegs;

	u64 deepOpcodeSuccessFreq[1024];
	u64 deepOpcodeSuspectFreq[1024];

	u32 diffMismatchRegSpecific[15];
	u32 diffMismatchFlagSpecific[4]; // 0:N, 1:Z, 2:C, 3:V
	u32 diffMismatchLengthBins[6];   // Correlates with blockLengthBins

	int mismatchCount = 0;
	int traceLogCount = 0;
	u32 framesRendered = 0;

	void reset();
	void print();
	void recordFPS(float coreFPS, float renderFPS);
	void commitFrameskip();
	void updateDRC(int unplayed, int newState);

	u32 diffCheckedPCHash[DIFF_PC_HASH_SIZE];

	bool isPCChecked(u32 pc);
	void markPCChecked(u32 pc);
};

extern DebugStats debugStats;

#endif // PROFILER_H
#endif

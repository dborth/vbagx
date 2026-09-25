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

enum PhaseId {
	PHASE_PPU = 0,   // CPURenderLine_Wii(), per scanline
	PHASE_SOUND,     // psoundTickfn(): APU synth + resample + ring write, per 1/100 s
	PHASE_INPUT,     // joypad read + motion sensor, per frame
	PHASE_SYSFRAME,  // systemFrame(): pacing/skip decision (+usleep in timer mode)
	PHASE_PRESENT,   // systemDrawScreen() -> presentFrame() (everything below + overlays)
	PHASE_UPLOAD,    //   uploadFrame(): RGB555 -> RGBA8 + GX2Invalidate
	PHASE_DRAW,      //   drawQuad(): record TV + DRC draw commands
	PHASE_SCANCOPY,  //   WHBGfxFinishRenderTV/DRC: copy to scan buffers
	PHASE_SWAPWAIT,  //   WHBGfxFinishRender: SwapScanBuffers+Flush+DrawDone (GPU + vblank wait)
	PHASE_PREPARE,   //   prepareFrame() for the next frame (holds the flip wait in sync mode)
	PHASE_GPUWAIT,   //   pipelined: wait for the GPU to retire the previous frame before CPU writes
	PHASE_FLIPWAIT,  //   pipelined: wait for the previous swap to flip (the vsync wait)
	PHASE_COUNT
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

	// ---- Phase timing (see PhaseId) ----
	struct PhaseStat {
		u64 ticks;      // run total
		u64 maxTicks;   // worst single call, whole run
		u64 ivMaxTicks; // worst single call, current log interval
		u32 calls;
		void clear() { ticks = 0; maxTicks = 0; ivMaxTicks = 0; calls = 0; }
		void add(u64 dt) {
			ticks += dt; calls++;
			if (dt > maxTicks) maxTicks = dt;
			if (dt > ivMaxTicks) ivMaxTicks = dt;
		}
	};
	PhaseStat ph[PHASE_COUNT];

	// ---- Frame-level timing ----
	u32 coreFrames;             // every emulated frame (rendered or skipped)
	u64 lastCoreFrameTick;
	u64 corePeriodMaxTicks;
	u32 corePeriodBins[8];      // ms between systemFrame() calls; edges in Profiler.cpp

	// Time from the previous present returning to this present starting.
	// Only sampled when no frame was skipped in between, so it is the cost
	// of one full emulated frame (CPU+PPU+sound+input) with no present in it.
	u64 presentEnterTick, lastPresentExitTick;
	u32 skipsAtLastPresentExit;
	u64 emuWorkTicks, emuWorkMaxTicks, ivEmuWorkMaxTicks, ivEmuWorkTicks;
	u32 emuWorkSamples, ivEmuWorkSamples;
	u32 emuWorkBins[7];         // as a fraction of the vsync period; edges in Profiler.cpp
	u32 vsyncsPerRenderBins[4]; // present-return to present-return, in vsyncs: 1, 2, 3, 4+

	// ---- Per-interval snapshot for the time-series log ----
	u64 snapWallTick, snapThumb, snapArm, snapJit, snapComp, snapPh[PHASE_COUNT];
	u32 snapCoreFrames, snapSkipped, snapAudioOverflow;
	u64 snapJitInstr, snapFbInstr, snapJitHops;
	u32 snapCompiles, snapFlushes, snapEvictions, snapSmc, snapSmcPatched;
	u32 intervalCount;
	u8  frameskipOn;            // EmuSettings.gbaFrameSkip, sampled in systemFrame()
	u32 vsyncUsHint;            // last vsync period passed to onPresentEnd()

	void onCoreFrame();
	void onPresentBegin();
	void onPresentEnd(u32 vsyncUs);
	void logInterval(float coreFPS, float renderFPS);
	void printPhases(double totalSecs);

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

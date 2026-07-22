#ifndef NO_JIT_COMPILER
#ifndef JIT_PROFILER_H
#define JIT_PROFILER_H
#include "../common/Port.h"

static const int MAX_JIT_TRACE_CALLS = 100;
static const int MAX_JIT_MISMATCH_COUNT = 100;

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

struct JITStats {
    u64 timeTotalStart;
    u64 timeTotalElapsed;
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

	int mismatchCount = 0;
	int traceLogCount = 0;
	u32 framesRendered = 0;

	void reset();
	void print();
};

extern JITStats jitStats;

#endif // JIT_PROFILER_H
#endif

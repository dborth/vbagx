#ifndef NO_JIT_COMPILER
#ifndef JIT_PROFILER_H
#define JIT_PROFILER_H
#include "../common/Port.h"

static const int MAX_JIT_TRACE_CALLS = 100;
static const int MAX_JIT_MISMATCH_COUNT = 100;

enum BailoutReason {
	BAILOUT_UNKNOWN = 0,
	BAILOUT_UNSUPPORTED_OPCODE,
	BAILOUT_BUFFER_OVERFLOW,
	BAILOUT_UNSUPPORTED_ALU,
	BAILOUT_UNSUPPORTED_MEM_BANK,
	BAILOUT_ARM_SWITCH,
	BAILOUT_CONDITIONAL_BRANCH,
	BAILOUT_BRANCH_WITH_LINK,
	BAILOUT_TMB9_10_11_INSTR_COUNT_ZERO,
	BAILOUT_THB14_INSTR_COUNT_ZERO,
	BAILOUT_THB15_INSTR_COUNT_ZERO,
	BAILOUT_UNKNOWN_MEM_OP,
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

	u64 jitInstructionsExecuted;
	u64 fallbackInstructionsExecuted;
	u32 blocksCompiled;

	u64 jitInvocations;
	u64 fallbackInvocations;
	u32 blockLengthBins[6];

	u64 fallbackOpcodeFreq[1024];
	u32 compileBailoutFreq[1024];
	u32 bailoutReasons[BAILOUT_REASON_COUNT];

	int mismatchCount = 0;
	int traceLogCount = 0;

	void reset();
	void print();
};

extern JITStats jitStats;

#endif // JIT_PROFILER_H
#endif

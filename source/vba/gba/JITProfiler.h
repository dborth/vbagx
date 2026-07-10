#ifndef JIT_PROFILER_H
#define JIT_PROFILER_H
#include "../common/Port.h"

enum BailoutReason {
	BAILOUT_UNKNOWN = 0,
	BAILOUT_BUFFER_OVERFLOW,
	BAILOUT_UNSUPPORTED_ALU,
	BAILOUT_UNSUPPORTED_MEM_BANK,
	BAILOUT_ARM_SWITCH,
	BAILOUT_CONDITIONAL_BRANCH,
	BAILOUT_BRANCH_WITH_LINK,
	BAILOUT_MEM_INSTR_COUNT_ZERO,
	BAILOUT_MEM,
	BAILOUT_REASON_COUNT
};

struct JITStats {
	u64 jitInstructionsExecuted;
	u64 fallbackInstructionsExecuted;
	u32 blocksCompiled;

	u64 fallbackOpcodeFreq[1024];
	u32 compileBailoutFreq[1024];
	u32 bailoutReasons[BAILOUT_REASON_COUNT];

	void reset();
	void print();
};
extern JITStats g_jitStats;
#if ENABLE_JIT_PROFILING
	// Clean macros to use inside the compiler/interpreter
	#define JIT_LOG_BLOCK_COMPILED()           g_jitStats.blocksCompiled++
	#define JIT_LOG_EXEC(count)                g_jitStats.jitInstructionsExecuted += (count)
	#define JIT_LOG_FALLBACK(opcode)           do { g_jitStats.fallbackInstructionsExecuted++; g_jitStats.fallbackOpcodeFreq[(opcode) >> 6]++; } while(0)
	#define JIT_LOG_BAILOUT(opcode, reason)    do { g_jitStats.compileBailoutFreq[(opcode) >> 6]++; g_jitStats.bailoutReasons[reason]++; } while(0)
#else
    // If disabled, these macros vanish and cost 0 CPU cycles
    #define JIT_LOG_BLOCK_COMPILED()
    #define JIT_LOG_EXEC(count)
    #define JIT_LOG_FALLBACK(opcode)
    #define JIT_LOG_BAILOUT(opcode, reason)
#endif
#endif

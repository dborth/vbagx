#ifndef JIT_PROFILER_H
#define JIT_PROFILER_H
#include "../common/Port.h"
#include "JITDebug.h"

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
	BAILOUT_HIGH_REGISTER_CMP,
	BAILOUT_REASON_COUNT
};

struct JITStats {
	u64 jitInstructionsExecuted;
	u64 fallbackInstructionsExecuted;
	u32 blocksCompiled;

	u64 fallbackOpcodeFreq[1024];
	u32 compileBailoutFreq[1024];
	u32 bailoutReasons[BAILOUT_REASON_COUNT];

	int mismatchCount = 0;
	int traceLogCount = 0;

	void reset();
	void print();
};

extern JITStats g_jitStats;

#if ENABLE_JIT_DEBUG_LOG
	// Profiling & Debug Logging Macros
	#define JIT_LOG(fmt, ...) \
		LogJIT(fmt, ##__VA_ARGS__)

	#define JIT_LOG_MISMATCH(msg) LogJITMismatch(msg)

	#define JIT_LOG_BLOCK_COMPILED(startPC) do { \
		g_jitStats.blocksCompiled++; \
		LogJITBlockCompileStart(startPC); \
	} while(0)

	#define JIT_LOG_BLOCK_COMPILE_END(startPC, endPC, instrCount, staticCycles, bailedOut, bailoutReason) \ \
		LogJITBlockCompileEnd((startPC), (endPC), (instrCount), (staticCycles), (bailedOut), (bailoutReason))

	#define JIT_LOG_INSN_COMPILED(pc, opcode, fmt, ...) \
		LogJITInsnCompiled((pc), (opcode), fmt, ##__VA_ARGS__)

	#define JIT_LOG_BAILOUT(pc, opcode, reason) do { \
		g_jitStats.compileBailoutFreq[(opcode) >> 6]++; \
		g_jitStats.bailoutReasons[reason]++; \
		LogJITBailout((pc), (opcode), #reason); \
	} while(0)

	#define JIT_LOG_EXEC(count) \
		g_jitStats.jitInstructionsExecuted += (count)

	#define JIT_LOG_FALLBACK(opcode) do { \
		g_jitStats.fallbackInstructionsExecuted++; \
		g_jitStats.fallbackOpcodeFreq[(opcode) >> 6]++; \
	} while(0)

	// Trace Execution Logging
	#define JIT_LOG_TRACE_ENTRY(pc, flags) \
		LogJITTraceExecution(true, (pc), 0, (flags), 0)

	#define JIT_LOG_TRACE_EXIT(pc, nextPC, flags, cycles) \
		LogJITTraceExecution(false, (pc), (nextPC), (flags), (cycles))

#else
	#define JIT_LOG(fmt, ...) 														((void)0)
	#define JIT_LOG_MISMATCH(msg)													((void)0)
	#define JIT_LOG_BLOCK_COMPILED(startPC)                							((void)0)
	#define JIT_LOG_BLOCK_COMPILE_END(startPC, endPC, count, cycles, bailed, rsn)	((void)0)
	#define JIT_LOG_INSN_COMPILED(pc, opcode, details, ...)     					((void)0)
	#define JIT_LOG_BAILOUT(pc, opcode, reason)            							((void)0)
	#define JIT_LOG_EXEC(count) 													((void)0)
	#define JIT_LOG_FALLBACK(opcode) 												((void)0)
	#define JIT_LOG_TRACE_ENTRY(pc, flags) 											((void)0)
	#define JIT_LOG_TRACE_EXIT(pc, nextPC, flags, cycles) 							((void)0)
#endif

#endif // JIT_PROFILER_H

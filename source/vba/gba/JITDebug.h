#ifndef JIT_DEBUG_H
#define JIT_DEBUG_H

#ifndef NO_JIT_COMPILER
	//#define JIT_PROFILING 1
	//#define JIT_BLOCK_FRAGMENTATION_STATS 1
	//#define JIT_DEBUG_BLOCK_DUMP 1
	//#define JIT_CACHE_AND_ARENA_LOG 1
	//#define JIT_COMPILER_DIFFERENTIAL_TESTING 1
	//#define JIT_DEBUGSTATELOG 1
	//#define JIT_DETAILED_LOG 1

	#include <ogc/timesupp.h>
	#include "JITProfiler.h"
	#include "../common/Types.h"

	struct BasicBlock;

	void InitJITLog();
	void LogJIT(const char* format, ...);
	void WriteJITLogToFile();
	void LogJITTraceExecution(bool isEntry, u32 entryPC, u32 nextPC, const u32 flags[4], u32 cycles);
	void LogJITMismatch(const char* msg);
	void LogJITBlockCompileStart(u32 startPC);
	void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...);
	void LogJITBailout(u32 pc, u32 opcode, const char* reasonName);
	void LogJITBlockCompileEnd(u32 startPC, u32 endPC, u32 instrCount, u32 staticCycles, bool bailedOut, u32 bailoutReason);
	void DebugDumpFirstJITBlock(BasicBlock* block);

	#if JIT_PROFILING
		#define JIT_LOG(fmt, ...) LogJIT(fmt, ##__VA_ARGS__)

		#define PROFILER_START_TIMER(name) u64 name = gettime()
		#define PROFILER_ADD_TIME(stat, name) jitStats.stat += (gettime() - (name))
		#define PROFILER_INC(stat) jitStats.stat++
		#define PROFILER_ADD(stat, val) jitStats.stat += (val)
		#define PROFILER_BIN_BLOCK(len) do { \
			if ((len) == 0) jitStats.blacklistedBlocks++; \
			else if ((len) <= 4) jitStats.blockLengthBins[0]++; \
			else if ((len) <= 8) jitStats.blockLengthBins[1]++; \
			else if ((len) <= 16) jitStats.blockLengthBins[2]++; \
			else if ((len) <= 32) jitStats.blockLengthBins[3]++; \
			else if ((len) <= 64) jitStats.blockLengthBins[4]++; \
			else jitStats.blockLengthBins[5]++; \
		} while(0)

		#define PROFILER_CACHE_HIT() jitStats.cacheHits++
		#define PROFILER_CACHE_MISS() jitStats.cacheMisses++
		#define PROFILER_CACHE_EVICT(evicted, pc) do { \
			if ((evicted) != 0 && (evicted) != (pc)) { \
				jitStats.cacheEvictions++; \
			} \
		} while(0)
		#define PROFILER_CACHE_FLUSH_START() u64 __flushTimer = gettime()
		#define PROFILER_CACHE_FLUSH_END() do { \
			jitStats.cacheFlushes++; \
			jitStats.timeSpentFlushing += (gettime() - __flushTimer); \
		} while(0)

		#define PROFILER_MARK_FRAME() jitStats.framesRendered++

		#define JIT_RESET_LOGS() do { \
			InitJITLog(); \
			jitStats.reset(); \
			JIT_LOG_STATE_INIT(); \
		} while(0)
		#define JIT_OUTPUT_LOGS() do { \
			jitStats.print(); \
			JIT_LOG_STATE_WRITE_TO_FILE(); \
		} while(0)

		#define JIT_LOG_BLOCK_COMPILED(startPC, block) do { \
			if (block != nullptr) { \
				jitStats.blocksCompiled++; \
				PROFILER_BIN_BLOCK(block->length); \
				JIT_LOG_BLOCK_COMPILED_DETAILS((startPC)); \
			} \
		} while(0)

		#define JIT_LOG_BAILOUT(pc, opcode, reason) do { \
			jitStats.compileBailoutFreq[(opcode) >> 6]++; \
			jitStats.bailoutReasons[reason]++; \
			JIT_LOG_BAILOUT_DETAILS((pc), (opcode), #reason); \
		} while(0)

		#if JIT_BLOCK_FRAGMENTATION_STATS
			#define PROFILER_FRAG_STATS(count, blockLen, bailed) do { \
				if (bailed) { \
					jitStats.partialBlockExecutions++; \
					if ((count) == 0) jitStats.bailoutOffsetBins[0]++; \
					else if ((count) <= 3) jitStats.bailoutOffsetBins[1]++; \
					else if ((count) <= 7) jitStats.bailoutOffsetBins[2]++; \
					else if ((count) <= 15) jitStats.bailoutOffsetBins[3]++; \
					else if ((count) <= 31) jitStats.bailoutOffsetBins[4]++; \
					else jitStats.bailoutOffsetBins[5]++; \
				} else { \
					jitStats.fullBlockCompletions++; \
				} \
				if ((blockLen) > 0) { \
					u32 pct = ((count) * 100) / (blockLen); \
					if (pct < 25) jitStats.blockExecutionRatioBins[0]++; \
					else if (pct < 50) jitStats.blockExecutionRatioBins[1]++; \
					else if (pct < 75) jitStats.blockExecutionRatioBins[2]++; \
					else if (pct < 100) jitStats.blockExecutionRatioBins[3]++; \
					else jitStats.blockExecutionRatioBins[4]++; \
				} \
			} while(0)
			#define PROFILER_DECLARE_BAILOUT_FLAG()       bool lastStepWasBailout = false
			#define PROFILER_CHECK_BAILOUT_TRANSITION()   do { if (lastStepWasBailout) { jitStats.bailoutToJitTransitions++; lastStepWasBailout = false; } } while(0)
			#define PROFILER_SET_BAILOUT_FLAG()           do { lastStepWasBailout = true; } while(0)
			#define PROFILER_CLEAR_BAILOUT_FLAG()         do { lastStepWasBailout = false; } while(0)
		#else
			#define PROFILER_FRAG_STATS(count, blockLen, bailed)		((void)0)
		#endif

		#define JIT_LOG_EXEC(count, blockLen, bailed) do { \
			jitStats.jitInstructionsExecuted += (count); \
			PROFILER_FRAG_STATS((count), (blockLen), (bailed)); \
		} while(0)
		#define JIT_LOG_FALLBACK(opcode) do { \
			jitStats.fallbackInvocations++; \
			jitStats.fallbackInstructionsExecuted++; \
			jitStats.fallbackOpcodeFreq[(opcode) >> 6]++; \
		} while(0)
	#endif // JIT_PROFILING

	#if JIT_DEBUG_BLOCK_DUMP
		#define JIT_DEBUG_DUMP_FIRST_JIT_BLOCK(block) DebugDumpFirstJITBlock((block))
	#endif

	#if JIT_CACHE_AND_ARENA_LOG
		#define JIT_LOG_CACHE_EVENT(bucket, startPC, evictedPC, arenaBefore, arenaAfter) do { \
			LogJIT("[CACHE] Bucket: %4u | Insert: 0x%08X | Evicted: 0x%08X | Arena: 0x%08X -> 0x%08X\n", \
				   (u32)(bucket), (u32)(startPC), (u32)(evictedPC), (u32)(arenaBefore), (u32)(arenaAfter)); \
		} while(0)

		#define JIT_LOG_CACHE_FLUSH() \
			LogJIT("[CACHE] FLUSH TRIGGERED - Arena Rewound to 0\n")

		#define JIT_LOG_ARENA(startPC, allocOffset, reserved, used, rewind) do { \
			LogJIT("[ARENA] Block 0x%08X | Offset: 0x%08X | Res: %u | Used: %u | Rewind: %u\n", \
				   (u32)(startPC), (u32)(allocOffset), (u32)(reserved), (u32)(used), (u32)(rewind)); \
		} while(0)
	#else
		#define JIT_LOG_CACHE_EVENT(bucket, startPC, evictedPC, arenaBefore, arenaAfter)	((void)0)
		#define JIT_LOG_CACHE_FLUSH()														((void)0)
		#define JIT_LOG_ARENA(startPC, allocOffset, reserved, used, rewind)					((void)0)
	#endif //JIT_CACHE_AND_ARENA_LOG

	#if JIT_DETAILED_LOG
		#define JIT_LOG_BLOCK_COMPILED_DETAILS(startPC) \
			LogJITBlockCompileStart((startPC))

		#define JIT_LOG_BLOCK_COMPILE_END(startPC, endPC, instrCount, staticCycles, bailedOut, bailoutReason) \
			LogJITBlockCompileEnd((startPC), (endPC), (instrCount), (staticCycles), (bailedOut), (bailoutReason))

		#define JIT_LOG_INSN_COMPILED(pc, opcode, fmt, ...) \
			LogJITInsnCompiled((pc), (opcode), fmt, ##__VA_ARGS__)

		#define JIT_LOG_TRACE_ENTRY(pc, flags) \
			LogJITTraceExecution(true, (pc), 0, (flags), 0)

		#define JIT_LOG_TRACE_EXIT(pc, nextPC, flags, cycles) \
			LogJITTraceExecution(false, (pc), (nextPC), (flags), (cycles))

		#define JIT_LOG_BAILOUT_DETAILS(pc, opcode, reason) \
			LogJITBailout((pc), (opcode), (reason))

		#define JIT_LOG_INSN_DUMP(pc, phase, addr, word) do { \
			LogJIT("[%s] PC: 0x%08X | Addr: 0x%p | Word: 0x%08X\n", \
				   (phase), (u32)(pc), (void*)(addr), (u32)(word)); \
		} while(0)
	#else
		#define JIT_LOG_BLOCK_COMPILED_DETAILS(startPC)										((void)0)
		#define JIT_LOG_BLOCK_COMPILE_END(startPC, endPC, count, cycles, bailed, rsn)		((void)0)
		#define JIT_LOG_INSN_COMPILED(pc, opcode, details, ...)     						((void)0)
		#define JIT_LOG_TRACE_ENTRY(pc, flags) 												((void)0)
		#define JIT_LOG_TRACE_EXIT(pc, nextPC, flags, cycles) 								((void)0)
		#define JIT_LOG_BAILOUT_DETAILS(pc, opcode, reason)									((void)0)
		#define JIT_LOG_INSN_DUMP(pc, phase, addr, word)									((void)0)
	#endif // JIT_DETAILED_LOG

	#if JIT_COMPILER_DIFFERENTIAL_TESTING
		#define JIT_LOG_MISMATCH(msg)														LogJITMismatch(msg)
	#endif
#endif // !NO_JIT_COMPILER

#ifndef PROFILER_START_TIMER
#define PROFILER_START_TIMER(name)					((void)0)
#endif
#ifndef PROFILER_ADD_TIME
#define PROFILER_ADD_TIME(stat, name)				((void)0)
#endif
#ifndef PROFILER_INC
#define PROFILER_INC(stat)							((void)0)
#endif
#ifndef PROFILER_ADD
#define PROFILER_ADD(stat, val)						((void)0)
#endif
#ifndef PROFILER_BIN_BLOCK
#define PROFILER_BIN_BLOCK(len)						((void)0)
#endif
#ifndef JIT_LOG
#define JIT_LOG(fmt, ...) 							((void)0)
#endif
#ifndef JIT_RESET_LOGS
#define JIT_RESET_LOGS()							((void)0)
#endif
#ifndef JIT_OUTPUT_LOGS
#define JIT_OUTPUT_LOGS()							((void)0)
#endif
#ifndef JIT_LOG_BLOCK_COMPILED
#define JIT_LOG_BLOCK_COMPILED(startPC, block)		((void)0)
#endif
#ifndef JIT_LOG_BAILOUT
#define JIT_LOG_BAILOUT(pc, opcode, reason)			((void)0)
#endif
#ifndef JIT_LOG_EXEC
#define JIT_LOG_EXEC(count, blockLen, bailed)			((void)0)
#endif
#ifndef PROFILER_DECLARE_BAILOUT_FLAG
#define PROFILER_DECLARE_BAILOUT_FLAG()       			((void)0)
#endif
#ifndef PROFILER_CHECK_BAILOUT_TRANSITION
#define PROFILER_CHECK_BAILOUT_TRANSITION()   			((void)0)
#endif
#ifndef PROFILER_SET_BAILOUT_FLAG
#define PROFILER_SET_BAILOUT_FLAG()           			((void)0)
#endif
#ifndef PROFILER_CLEAR_BAILOUT_FLAG
#define PROFILER_CLEAR_BAILOUT_FLAG()         			((void)0)
#endif
#ifndef JIT_LOG_FALLBACK
#define JIT_LOG_FALLBACK(opcode)					((void)0)
#endif
#ifndef JIT_DEBUG_DUMP_FIRST_JIT_BLOCK
#define JIT_DEBUG_DUMP_FIRST_JIT_BLOCK(block)		((void)0)
#endif
#ifndef PROFILER_MARK_FRAME
#define PROFILER_MARK_FRAME()                       ((void)0)
#endif

#ifndef PROFILER_CACHE_HIT
#define PROFILER_CACHE_HIT()                        ((void)0)
#endif
#ifndef PROFILER_CACHE_MISS
#define PROFILER_CACHE_MISS()                       ((void)0)
#endif
#ifndef PROFILER_CACHE_EVICT
#define PROFILER_CACHE_EVICT(evicted, pc)           ((void)0)
#endif
#ifndef PROFILER_CACHE_FLUSH_START
#define PROFILER_CACHE_FLUSH_START()                ((void)0)
#endif
#ifndef PROFILER_CACHE_FLUSH_END
#define PROFILER_CACHE_FLUSH_END()                  ((void)0)
#endif

#ifndef JIT_LOG_STATE_INIT
	#define JIT_LOG_STATE_INIT()        											((void)0)
#endif
#ifndef JIT_LOG_STATE_CPP
	#define JIT_LOG_STATE_CPP(executedPC, nextPC, ticks, cycles)       				((void)0)
#endif
#ifndef JIT_LOG_STATE_JIT
    #define JIT_LOG_STATE_JIT(executedPC, nextPC, ticks, cycles, instrCount)        ((void)0)
#endif
#ifndef JIT_LOG_STATE_WRITE_TO_FILE
	#define JIT_LOG_STATE_WRITE_TO_FILE()											((void)0)
#endif
#endif // JIT_DEBUG_H

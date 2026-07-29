/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * Debug.h
 *
 * The single gating point for every JIT debug/profiling facility.
 * Nothing outside this header decides whether instrumentation is compiled
 * in - every call site elsewhere in the JIT (JITCompiler.cpp, JITCache.cpp,
 * etc.) just calls one of the macros below unconditionally, and this header
 * decides whether it costs anything.
 *
 * Structure: each independently-toggleable feature (real-time profiling
 * stats, cache/arena event tracing, per-instruction detailed compile/exec
 * logging, differential mismatch testing, full per-instruction state-log
 * dumping, first-block disassembly dumping) is gated behind its own
 * `#define` at the top (JIT_CACHE_AND_ARENA_LOG, JIT_DIFFERENTIAL_TESTING,
 * JIT_DEBUGSTATELOG, JIT_DETAILED_LOG, etc.), with a real macro definition
 * when the feature is enabled and a `((void)0)` no-op fallback (at the
 * bottom of the file) when it isn't or when VBAGX_DEBUG isn't defined at
 * all - so a release build carries exactly zero instrumentation cost.
 *
 * The macros here are the plumbing for:
 *   - debugStats: real-time profiler counters (bailout reasons,
 *     cache hit/miss/eviction/flush counts, block-length histograms,
 *     compile-vs-execute wall-clock split, MIPS, etc.) — see Profiler.h.
 *   - JIT_LOG_CACHE_EVENT / JIT_LOG_ARENA / JIT_LOG_CACHE_FLUSH: arena and
 *     hash-bucket tracing for JITCache.cpp.
 *   - JIT_LOG_*_DETAILS / LogJIT*(): the per-instruction/per-block detailed
 *     text log (JITCompiler.cpp compile-time tracing, trace entry/exit).
 *   - JIT_LOG_MISMATCH: routes into JITDifferential.cpp's mismatch reporter.
 *   - JIT_LOG_STATE_*: routes into JITDebugStateLog for the full per-
 *     instruction state dump used by align_traces.py.
 *   - DebugDumpFirstJITBlock/JIT_DEBUG_DUMP_FIRST_JIT_BLOCK: one-shot raw
 *     disassembly dump of the first compiled block, for objdump inspection.
 ***************************************************************************/

#ifndef DEBUG_H
#define DEBUG_H

#ifdef VBAGX_DEBUG
	//#define PROFILING 1
	//#define JIT_BLOCK_FRAGMENTATION_STATS 1
	//#define JIT_DEBUG_BLOCK_DUMP 1
	//#define JIT_CACHE_AND_ARENA_LOG 1
	//#define JIT_DIFFERENTIAL_TESTING 1
	//#define JIT_DEBUGSTATELOG 1
	//#define JIT_DETAILED_LOG 1

	#include <ogc/timesupp.h>
	#include "GBA.h"
	#include "Profiler.h"
	#include "JITDebugStateLog.h"
	#include "../common/Types.h"

	struct BasicBlock;

	void InitDebugLog();
	void LogDebug(const char* format, ...);
	void WriteDebugLogToFile();
	void LogJITTraceExecution(bool isEntry, u32 entryPC, u32 nextPC, CPUFlags flags, u32 cycles);
	void LogJITMismatch(const char* msg);
	void LogJITBlockCompileStart(u32 startPC);
	void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...);
	void LogJITBailout(u32 pc, u32 opcode, const char* reasonName);
	void LogJITBlockCompileEnd(u32 startPC, u32 endPC, u32 instrCount, u32 staticCycles, bool bailedOut, u32 bailoutReason);
	void DebugDumpFirstJITBlock(BasicBlock* block);

	#if PROFILING
		#define DEBUG_LOG(fmt, ...) LogDebug(fmt, ##__VA_ARGS__)

		#define PROFILER_LOG_FPS(coreFPS, displayFPS) debugStats.recordFPS(coreFPS, displayFPS)
		#define PROFILER_COMMIT_FRAMESKIP()           debugStats.commitFrameskip()

		#define PROFILER_LOG_AUDIO_STARVATION()       debugStats.audioStarvationEvents++
		#define PROFILER_LOG_AUDIO_OVERFLOW()         debugStats.audioOverflowDrops++
		#define PROFILER_LOG_DRC(unplayed, newState)  debugStats.updateDRC(unplayed, (int)newState)

		#define PROFILER_START_TIMER(name) u64 name = gettime()
		#define PROFILER_ADD_TIME(stat, name) debugStats.stat += (gettime() - (name))
		#define PROFILER_INC(stat) debugStats.stat++
		#define PROFILER_ADD(stat, val) debugStats.stat += (val)
		#define PROFILER_BIN_BLOCK(len) do { \
			if ((len) == 0) debugStats.blacklistedBlocks++; \
			else if ((len) <= 4) debugStats.blockLengthBins[0]++; \
			else if ((len) <= 8) debugStats.blockLengthBins[1]++; \
			else if ((len) <= 16) debugStats.blockLengthBins[2]++; \
			else if ((len) <= 32) debugStats.blockLengthBins[3]++; \
			else if ((len) <= 64) debugStats.blockLengthBins[4]++; \
			else debugStats.blockLengthBins[5]++; \
		} while(0)

		#define PROFILER_CACHE_HIT() debugStats.cacheHits++
		#define PROFILER_CACHE_MISS() debugStats.cacheMisses++
		#define PROFILER_CACHE_EVICT(evicted, pc) do { \
			if ((evicted) != 0 && (evicted) != (pc)) { \
				debugStats.cacheEvictions++; \
			} \
		} while(0)
		#define PROFILER_CACHE_FLUSH_START() u64 __flushTimer = gettime()
		#define PROFILER_CACHE_FLUSH_END() do { \
			debugStats.cacheFlushes++; \
			debugStats.timeSpentFlushing += (gettime() - __flushTimer); \
		} while(0)

		#define PROFILER_MARK_FRAME() debugStats.framesRendered++

		#define DEBUG_RESET_LOGS() do { \
			InitDebugLog(); \
			debugStats.reset(); \
			JIT_LOG_STATE_INIT(); \
		} while(0)
		#define DEBUG_OUTPUT_LOGS() do { \
			debugStats.print(); \
			JIT_LOG_STATE_WRITE_TO_FILE(); \
		} while(0)

		#define JIT_LOG_BLOCK_COMPILED(startPC, block) do { \
			if (block != nullptr) { \
				debugStats.blocksCompiled++; \
				PROFILER_BIN_BLOCK(block->length); \
				JIT_LOG_BLOCK_COMPILED_DETAILS((startPC)); \
			} \
		} while(0)

		#define JIT_LOG_BAILOUT(pc, opcode, reason) do { \
			debugStats.compileBailoutFreq[(opcode) >> 6]++; \
			debugStats.bailoutReasons[reason]++; \
			JIT_LOG_BAILOUT_DETAILS((pc), (opcode), #reason); \
		} while(0)

		#if JIT_BLOCK_FRAGMENTATION_STATS
			#define PROFILER_FRAG_STATS(count, blockLen, bailed) do { \
				if (bailed) { \
					debugStats.partialBlockExecutions++; \
					if ((count) == 0) debugStats.bailoutOffsetBins[0]++; \
					else if ((count) <= 3) debugStats.bailoutOffsetBins[1]++; \
					else if ((count) <= 7) debugStats.bailoutOffsetBins[2]++; \
					else if ((count) <= 15) debugStats.bailoutOffsetBins[3]++; \
					else if ((count) <= 31) debugStats.bailoutOffsetBins[4]++; \
					else debugStats.bailoutOffsetBins[5]++; \
				} else { \
					debugStats.fullBlockCompletions++; \
				} \
				if ((blockLen) > 0) { \
					u32 pct = ((count) * 100) / (blockLen); \
					if (pct < 25) debugStats.blockExecutionRatioBins[0]++; \
					else if (pct < 50) debugStats.blockExecutionRatioBins[1]++; \
					else if (pct < 75) debugStats.blockExecutionRatioBins[2]++; \
					else if (pct < 100) debugStats.blockExecutionRatioBins[3]++; \
					else debugStats.blockExecutionRatioBins[4]++; \
				} \
			} while(0)
			#define PROFILER_DECLARE_BAILOUT_FLAG()       bool lastStepWasBailout = false
			#define PROFILER_CHECK_BAILOUT_TRANSITION()   do { if (lastStepWasBailout) { debugStats.bailoutToJitTransitions++; lastStepWasBailout = false; } } while(0)
			#define PROFILER_SET_BAILOUT_FLAG()           do { lastStepWasBailout = true; } while(0)
			#define PROFILER_CLEAR_BAILOUT_FLAG()         do { lastStepWasBailout = false; } while(0)
		#else
			#define PROFILER_FRAG_STATS(count, blockLen, bailed)		((void)0)
		#endif

		#define JIT_LOG_EXEC(count, blockLen, bailed) do { \
			debugStats.jitInstructionsExecuted += (count); \
			PROFILER_FRAG_STATS((count), (blockLen), (bailed)); \
		} while(0)
		#define JIT_LOG_FALLBACK(opcode) do { \
			debugStats.fallbackInvocations++; \
			debugStats.fallbackInstructionsExecuted++; \
			debugStats.fallbackOpcodeFreq[(opcode) >> 6]++; \
		} while(0)
	#endif // PROFILING

	#if JIT_DEBUG_BLOCK_DUMP
		#define JIT_DEBUG_DUMP_FIRST_JIT_BLOCK(block) DebugDumpFirstJITBlock((block))
	#endif

	#if JIT_CACHE_AND_ARENA_LOG
		#define JIT_LOG_CACHE_EVENT(bucket, startPC, evictedPC, arenaBefore, arenaAfter) do { \
			LogDebug("[CACHE] Bucket: %4u | Insert: 0x%08X | Evicted: 0x%08X | Arena: 0x%08X -> 0x%08X\n", \
				   (u32)(bucket), (u32)(startPC), (u32)(evictedPC), (u32)(arenaBefore), (u32)(arenaAfter)); \
		} while(0)

		#define JIT_LOG_CACHE_FLUSH() \
			LogDebug("[CACHE] FLUSH TRIGGERED - Arena Rewound to 0\n")

		#define JIT_LOG_ARENA(startPC, allocOffset, reserved, used, rewind) do { \
			LogDebug("[ARENA] Block 0x%08X | Offset: 0x%08X | Res: %u | Used: %u | Rewind: %u\n", \
				   (u32)(startPC), (u32)(allocOffset), (u32)(reserved), (u32)(used), (u32)(rewind)); \
		} while(0)
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
			LogDebug("[%s] PC: 0x%08X | Addr: 0x%p | Word: 0x%08X\n", \
				   (phase), (u32)(pc), (void*)(addr), (u32)(word)); \
		} while(0)
	#endif // JIT_DETAILED_LOG

	#if JIT_DIFFERENTIAL_TESTING
		#define JIT_LOG_MISMATCH(msg)														LogJITMismatch(msg)
	#endif

	#if JIT_DEBUGSTATELOG
		#define JIT_LOG_STATE_INIT()														jitDebugStateLog.Init()
		#define JIT_LOG_STATE_CPP(executedPC, nextPC, ticks, cycles)						jitDebugStateLog.LogState("[C++]", (executedPC), (nextPC), (ticks), (cycles), debugStats.fallbackInstructionsExecuted)
		#define JIT_LOG_STATE_JIT(executedPC, nextPC, ticks, cycles)						jitDebugStateLog.LogState("[JIT]", (executedPC), (nextPC), (ticks), (cycles), debugStats.fallbackInstructionsExecuted+debugStats.jitInstructionsExecuted)
		#define JIT_LOG_STATE_WRITE_TO_FILE()												jitDebugStateLog.WriteToFile()
	#endif

#endif // VBAGX_DEBUG

#ifndef PROFILER_LOG_FPS
#define PROFILER_LOG_FPS(coreFPS, displayFPS)		((void)0)
#endif
#ifndef PROFILER_COMMIT_FRAMESKIP
#define PROFILER_COMMIT_FRAMESKIP()					((void)0)
#endif
#ifndef PROFILER_LOG_AUDIO_STARVATION
#define PROFILER_LOG_AUDIO_STARVATION()				((void)0)
#endif
#ifndef PROFILER_LOG_AUDIO_OVERFLOW
#define PROFILER_LOG_AUDIO_OVERFLOW()				((void)0)
#endif
#ifndef PROFILER_LOG_DRC
#define PROFILER_LOG_DRC(unplayed, newState)		((void)0)
#endif
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
#ifndef DEBUG_LOG
#define DEBUG_LOG(fmt, ...) 						((void)0)
#endif
#ifndef DEBUG_RESET_LOGS
#define DEBUG_RESET_LOGS()							((void)0)
#endif
#ifndef DEBUG_OUTPUT_LOGS
#define DEBUG_OUTPUT_LOGS()							((void)0)
#endif
#ifndef JIT_LOG_BLOCK_COMPILED
#define JIT_LOG_BLOCK_COMPILED(startPC, block)		((void)0)
#endif
#ifndef JIT_LOG_BAILOUT
#define JIT_LOG_BAILOUT(pc, opcode, reason)			((void)0)
#endif
#ifndef JIT_LOG_EXEC
#define JIT_LOG_EXEC(count, blockLen, bailed)		((void)0)
#endif
#ifndef PROFILER_DECLARE_BAILOUT_FLAG
#define PROFILER_DECLARE_BAILOUT_FLAG()       		((void)0)
#endif
#ifndef PROFILER_CHECK_BAILOUT_TRANSITION
#define PROFILER_CHECK_BAILOUT_TRANSITION()   		((void)0)
#endif
#ifndef PROFILER_SET_BAILOUT_FLAG
#define PROFILER_SET_BAILOUT_FLAG()           		((void)0)
#endif
#ifndef PROFILER_CLEAR_BAILOUT_FLAG
#define PROFILER_CLEAR_BAILOUT_FLAG()         		((void)0)
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

#ifndef JIT_LOG_BLOCK_COMPILED_DETAILS
#define JIT_LOG_BLOCK_COMPILED_DETAILS(startPC)										((void)0)
#endif
#ifndef JIT_LOG_BLOCK_COMPILE_END
#define JIT_LOG_BLOCK_COMPILE_END(startPC, endPC, count, cycles, bailed, rsn)		((void)0)
#endif
#ifndef JIT_LOG_INSN_COMPILED
#define JIT_LOG_INSN_COMPILED(pc, opcode, details, ...)     						((void)0)
#endif
#ifndef JIT_LOG_TRACE_ENTRY
#define JIT_LOG_TRACE_ENTRY(pc, flags) 												((void)0)
#endif
#ifndef JIT_LOG_TRACE_EXIT
#define JIT_LOG_TRACE_EXIT(pc, nextPC, flags, cycles) 								((void)0)
#endif
#ifndef JIT_LOG_BAILOUT_DETAILS
#define JIT_LOG_BAILOUT_DETAILS(pc, opcode, reason)									((void)0)
#endif
#ifndef JIT_LOG_INSN_DUMP
#define JIT_LOG_INSN_DUMP(pc, phase, addr, word)									((void)0)
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
#ifndef JIT_LOG_CACHE_EVENT
#define JIT_LOG_CACHE_EVENT(bucket, startPC, evictedPC, arenaBefore, arenaAfter)	((void)0)
#endif
#ifndef JIT_LOG_CACHE_FLUSH
#define JIT_LOG_CACHE_FLUSH()														((void)0)
#endif
#ifndef JIT_LOG_ARENA
#define JIT_LOG_ARENA(startPC, allocOffset, reserved, used, rewind)					((void)0)
#endif
#ifndef JIT_LOG_STATE_INIT
#define JIT_LOG_STATE_INIT()        												((void)0)
#endif
#ifndef JIT_LOG_STATE_CPP
#define JIT_LOG_STATE_CPP(executedPC, nextPC, ticks, cycles)       					((void)0)
#endif
#ifndef JIT_LOG_STATE_JIT
#define JIT_LOG_STATE_JIT(executedPC, nextPC, ticks, cycles)       	 	((void)0)
#endif
#ifndef JIT_LOG_STATE_WRITE_TO_FILE
#define JIT_LOG_STATE_WRITE_TO_FILE()												((void)0)
#endif
#ifndef JIT_DIFFERENTIAL_THUMB_HOOK
#define JIT_DIFFERENTIAL_THUMB_HOOK(pc, block) 										((void)0)
#endif
#endif // DEBUG_H

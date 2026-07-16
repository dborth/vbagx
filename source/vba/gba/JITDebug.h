#ifndef JIT_DEBUGLOG_H
#define JIT_DEBUGLOG_H

#include "JITDebugStateLog.h"
#include "JITProfiler.h"
#include "../common/Types.h"
#include <cstdarg>

struct JITRegionConfig {
    bool enableBIOS;   // 0x00 (16KB System ROM)
    bool enableEWRAM;  // 0x02 (256KB External WRAM)
    bool enableIWRAM;  // 0x03 (32KB Internal WRAM)
    bool enableROM0;   // 0x08, 0x09 (Game Pak ROM - Wait State 0)
    bool enableROM1;   // 0x0A, 0x0B (Game Pak ROM - Wait State 1)
    bool enableROM2;   // 0x0C, 0x0D (Game Pak ROM - Wait State 2)
};

bool JITRegionAllowed(u32 opcode);
void LogJIT(const char* format, ...);
void WriteJITLogToFile();
void LogJITTraceExecution(bool isEntry, u32 entryPC, u32 nextPC, const u32 flags[4], u32 cycles);
void LogJITMismatch(const char* msg);
void LogJITBlockCompileStart(u32 startPC);
void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...);
void LogJITBailout(u32 pc, u32 opcode, const char* reasonName);
void LogJITBlockCompileEnd(u32 startPC, u32 endPC, u32 instrCount, u32 staticCycles, bool bailedOut, u32 bailoutReason);


#if JIT_DEBUG
	#define JIT_RESET_LOGS() do { \
		JIT_RESET_STATS(); \
		JIT_LOG_STATE_INIT(); \
	} while(0)
	#define JIT_OUTPUT_LOGS() do { \
		JIT_PRINT_STATS(); \
		JIT_LOG_STATE_WRITE_TO_FILE(); \
	} while(0)
	#define JIT_REGION_ALLOWED(opcode) JITRegionAllowed(opcode)

	// Profiling & Debug Logging Macros
	#define JIT_LOG(fmt, ...) \
		LogJIT(fmt, ##__VA_ARGS__)

	#define JIT_LOG_MISMATCH(msg) LogJITMismatch(msg)

	#define JIT_LOG_BLOCK_COMPILED(startPC) do { \
		jitStats.blocksCompiled++; \
		LogJITBlockCompileStart(startPC); \
	} while(0)

	#define JIT_LOG_BLOCK_COMPILE_END(startPC, endPC, instrCount, staticCycles, bailedOut, bailoutReason) \ \
		LogJITBlockCompileEnd((startPC), (endPC), (instrCount), (staticCycles), (bailedOut), (bailoutReason))

	#define JIT_LOG_INSN_COMPILED(pc, opcode, fmt, ...) \
		LogJITInsnCompiled((pc), (opcode), fmt, ##__VA_ARGS__)

	#define JIT_LOG_BAILOUT(pc, opcode, reason) do { \
		jitStats.compileBailoutFreq[(opcode) >> 6]++; \
		jitStats.bailoutReasons[reason]++; \
		LogJITBailout((pc), (opcode), #reason); \
	} while(0)

	#define JIT_LOG_EXEC(count) \
		jitStats.jitInstructionsExecuted += (count)

	#define JIT_LOG_FALLBACK(opcode) do { \
		jitStats.fallbackInstructionsExecuted++; \
		jitStats.fallbackOpcodeFreq[(opcode) >> 6]++; \
	} while(0)

	// Trace Execution Logging
	#define JIT_LOG_TRACE_ENTRY(pc, flags) \
		LogJITTraceExecution(true, (pc), 0, (flags), 0)

	#define JIT_LOG_TRACE_EXIT(pc, nextPC, flags, cycles) \
		LogJITTraceExecution(false, (pc), (nextPC), (flags), (cycles))

	#define JIT_LOG_CACHE_EVENT(bucket, startPC, evictedPC, arenaBefore, arenaAfter) do { \
		LogJIT("[CACHE] Bucket: %4u | Insert: 0x%08X | Evicted: 0x%08X | Arena: 0x%08X -> 0x%08X\n", \
			   (u32)(bucket), (u32)(startPC), (u32)(evictedPC), (u32)(arenaBefore), (u32)(arenaAfter)); \
	} while(0)

	#define JIT_LOG_CACHE_FLUSH() do { \
		LogJIT("[CACHE] FLUSH TRIGGERED - Arena Rewound to 0\n"); \
	} while(0)

	#define JIT_LOG_ARENA(startPC, allocOffset, reserved, used, rewind) do { \
		LogJIT("[ARENA] Block 0x%08X | Offset: 0x%08X | Res: %u | Used: %u | Rewind: %u\n", \
			   (u32)(startPC), (u32)(allocOffset), (u32)(reserved), (u32)(used), (u32)(rewind)); \
	} while(0)

	#define JIT_LOG_INSN_DUMP(pc, phase, addr, word) do { \
		LogJIT("[%s] PC: 0x%08X | Addr: 0x%p | Word: 0x%08X\n", \
			   (phase), (u32)(pc), (void*)(addr), (u32)(word)); \
	} while(0)
#else
	#define JIT_RESET_LOGS()															((void)0)
	#define JIT_OUTPUT_LOGS()															((void)0)
	#define JIT_REGION_ALLOWED(opcode)													((void)(opcode), true)
	#define JIT_LOG(fmt, ...) 															((void)0)
	#define JIT_LOG_MISMATCH(msg)														((void)0)
	#define JIT_LOG_BLOCK_COMPILED(startPC)                								((void)0)
	#define JIT_LOG_BLOCK_COMPILE_END(startPC, endPC, count, cycles, bailed, rsn)		((void)0)
	#define JIT_LOG_INSN_COMPILED(pc, opcode, details, ...)     						((void)0)
	#define JIT_LOG_BAILOUT(pc, opcode, reason)            								((void)0)
	#define JIT_LOG_EXEC(count) 														((void)0)
	#define JIT_LOG_FALLBACK(opcode) 													((void)0)
	#define JIT_LOG_TRACE_ENTRY(pc, flags) 												((void)0)
	#define JIT_LOG_TRACE_EXIT(pc, nextPC, flags, cycles) 								((void)0)
	#define JIT_LOG_CACHE_EVENT(bucket, startPC, evictedPC, arenaBefore, arenaAfter)	((void)0)
	#define JIT_LOG_CACHE_FLUSH()														((void)0)
	#define JIT_LOG_ARENA(startPC, allocOffset, reserved, used, rewind)					((void)0)
	#define JIT_LOG_INSN_DUMP(pc, phase, addr, word)									((void)0)
#endif

#endif // JIT_DEBUGLOG_H

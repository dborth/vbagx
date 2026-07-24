#ifndef JIT_DEBUGSTATELOG_H
#define JIT_DEBUGSTATELOG_H

#ifndef NO_JIT_COMPILER
#if JIT_DEBUGSTATELOG
#include "../common/Types.h"

class JITDebugStateLog {
	private:
		u32 logCount = 0;
		char logPath[128] = "";
		char* logBuffer = nullptr;
		u32 currentOffset = 0;

	public:
		/**
		 * Logs the complete GBA core state
		 * @param source     	"[C++]" or "[JIT]"
		 * @param executedPC 	The PC address that just executed
		 * @param nextPC     	The next PC value to execute
		 * @param ticks      	Current tick accumulator state
		 * @param cycles     	Cycles
		 * @param instrCount    Total instruction count
		 */
		void Init(char * logType);
		void LogState(const char* source, u32 executedPC, u32 nextPC, u32 ticks, u32 cycles, u32 instrCount);
		void WriteToFile();
};

extern JITDebugStateLog jitDebugStateLog;

	#if JIT_COMPILER
	#define JIT_LOG_STATE_INIT()													jitDebugStateLog.Init("jit")
	#else
	#define JIT_LOG_STATE_INIT()													jitDebugStateLog.Init("interp")
	#endif
	#define JIT_LOG_STATE_CPP(executedPC, nextPC, ticks, cycles)					jitDebugStateLog.LogState("[C++]", (executedPC), (nextPC), (ticks), (cycles), 1, jitStats.fallbackInstructionsExecuted)
    #define JIT_LOG_STATE_JIT(executedPC, nextPC, ticks, cycles, instrCount)		jitDebugStateLog.LogState("[JIT]", (executedPC), (nextPC), (ticks), (cycles), (instrCount), jitStats.jitInstructionsExecuted)
	#define JIT_LOG_STATE_WRITE_TO_FILE()											jitDebugStateLog.WriteToFile()
#endif
#endif // NO_JIT_COMPILER

#endif // JIT_DEBUGSTATELOG_H

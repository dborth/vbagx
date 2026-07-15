#ifndef JIT_DEBUGSTATELOG_H
#define JIT_DEBUGSTATELOG_H

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
		 * @param source     "[C++]" or "[JIT]"
		 * @param executedPC The PC address that just executed
		 * @param nextPC     The next PC value to execute
		 * @param ticks      Current tick accumulator state
		 * @param cycles     Cycles
		 */
		void Init();
		void LogState(const char* source, u32 executedPC, u32 nextPC, u32 ticks, u32 cycles);
		void WriteToFile();
};

extern JITDebugStateLog jitDebugStateLog;

#if JIT_DEBUGSTATELOG
	#define JIT_LOG_STATE_INIT()										jitDebugStateLog.Init()
	#define JIT_LOG_STATE_CPP(executedPC, nextPC, ticks, cycles)		jitDebugStateLog.LogState("[C++]", (executedPC), (nextPC), (ticks), (cycles))
    #define JIT_LOG_STATE_JIT(executedPC, nextPC, ticks, cycles)		jitDebugStateLog.LogState("[JIT]", (executedPC), (nextPC), (ticks), (cycles))
	#define JIT_LOG_STATE_WRITE_TO_FILE()								jitDebugStateLog.WriteToFile()
#else
	#define JIT_LOG_STATE_INIT()        								((void)0)
	#define JIT_LOG_STATE_CPP(executedPC, nextPC, ticks, cycles)        ((void)0)
    #define JIT_LOG_STATE_JIT(executedPC, nextPC, ticks, cycles)        ((void)0)
	#define JIT_LOG_STATE_WRITE_TO_FILE()								((void)0)
#endif

#endif // JIT_DEBUGLOG_H

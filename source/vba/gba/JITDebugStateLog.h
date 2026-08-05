/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * JITDebugStateLog.h
 *
 * A "state-log alignment" differential-testing approach: two
 * full emulation runs (one with the JIT compiler disabled, one enabled) are
 * each logged independently to their own SD-card text file, then compared
 * offline by align_traces.py to find the first point of divergence. Because
 * JIT can do long runs, you may want to reduce JIT_TRACE_MAX_INSTRUCTIONS
 * and YIELD_NUMBER to help you track down mismatched instructions.
 *
 * A single global instance (jitDebugStateLog) is used by both the JIT-enabled
 * and interpreter-only code paths via the JIT_LOG_STATE_CPP/JIT_LOG_STATE_JIT
 * macros in Debug.h — the `source` string ("[C++]" vs "[JIT]") distinguishes
 * which path produced a given line, since the JIT path logs once per
 * compiled-block execution (not once per instruction, since a compiled block
 * has no per-instruction interpreter hook to attach to).
 ***************************************************************************/

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
		void Init();
		/**
		 * Logs the complete GBA core state
		 * @param source     	"[C++]" or "[JIT]"
		 * @param executedPC 	The PC address that just executed
		 * @param nextPC     	The next PC value to execute
		 * @param ticks      	Current tick accumulator state
		 * @param cycles     	Cycles
		 */
		void LogState(const char* source, u32 executedPC, u32 nextPC, u32 ticks, u32 cycles);
		void WriteToFile();
};

extern JITDebugStateLog jitDebugStateLog;

#endif // JIT_DEBUGSTATELOG_H

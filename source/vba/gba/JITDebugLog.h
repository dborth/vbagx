#ifndef JIT_DEBUGLOG_H
#define JIT_DEBUGLOG_H

#include "../common/Types.h"
#include <cstdarg>

void LogJIT(const char* format, ...);
void WriteJITLogToFile();
void LogJITTraceExecution(bool isEntry, u32 entryPC, u32 nextPC, const u32 flags[4], u32 cycles);
void LogJITMismatch(const char* format, ...);
void LogJITBlockCompileStart(u32 startPC);
void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...);
void LogJITBailout(u32 pc, u32 opcode, const char* reasonName);
void LogJITBlockCompileEnd(u32 startPC, u32 endPC, u32 instrCount, u32 staticCycles, bool bailedOut, u32 bailoutReason);

#endif // JIT_DEBUGLOG_H
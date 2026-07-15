#ifndef JIT_DEBUGLOG_H
#define JIT_DEBUGLOG_H

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
	#define JIT_REGION_ALLOWED(opcode) JITRegionAllowed(opcode)
#else
	#define JIT_REGION_ALLOWED(opcode) ((void)(opcode), true)
#endif

#endif // JIT_DEBUGLOG_H

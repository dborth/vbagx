#ifndef NO_JIT_COMPILER
#include "JITDebug.h"

#include "JITProfiler.h"

#include <time.h>
#include <ogc/system.h>
#include <string>
#include <cstdarg>
#include <cstdio>

static JITRegionConfig jitRegions = {
	true,  // enableBIOS
	true,  // enableEWRAM
	true,  // enableIWRAM
	true,  // enableROM0
    true,  // enableROM1
	true   // enableROM2
};

bool JITRegionAllowed(u32 opcode) {
	u8 bank = opcode >> 24;

	bool allowed = false;

	if (bank == 0x00) allowed = jitRegions.enableBIOS;
	else if (bank == 0x02) allowed = jitRegions.enableEWRAM;
	else if (bank == 0x03) allowed = jitRegions.enableIWRAM;
	else if (bank == 0x08 || bank == 0x09) allowed = jitRegions.enableROM0;
	else if (bank == 0x0A || bank == 0x0B) allowed = jitRegions.enableROM1;
	else if (bank == 0x0C || bank == 0x0D) allowed = jitRegions.enableROM2;

	return allowed;
}

// -----------------------------------------------------------------------------
// JIT Trace Logger Buffer & Utility Method
// -----------------------------------------------------------------------------
static std::string g_jitLogBuffer;
static char tmpBuffer[2048];

static void vLogJITInternal(const char* format, va_list args) {
	int written = vsnprintf(tmpBuffer, sizeof(tmpBuffer), format, args);
	if (written > 0) {
		g_jitLogBuffer.append(tmpBuffer, written);
	}
}

void LogJIT(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vLogJITInternal(format, args);
	va_end(args);
}

void LogJITMismatch(const char* message) {
	jitStats.mismatchCount++;
	int written = snprintf(tmpBuffer, sizeof(tmpBuffer),
						   "==================== [JIT DIFFERENTIAL MISMATCH #%d] ====================\n",
						   jitStats.mismatchCount);

	g_jitLogBuffer.append(tmpBuffer, written);
	g_jitLogBuffer.append(message);
	g_jitLogBuffer.append("========================================================================\n");
}

void LogJITBlockCompileStart(u32 startPC) {
    LogJIT("=== COMPILING JIT BLOCK @ 0x%08X ===\n", startPC);
}

void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...) {
	int written = snprintf(tmpBuffer, sizeof(tmpBuffer), "  [0x%08X] Opcode: 0x%04X | ", pc, opcode);
    g_jitLogBuffer.append(tmpBuffer, written);
    va_list args;
    va_start(args, format);
    vLogJITInternal(format, args);
    va_end(args);
    g_jitLogBuffer.append("\n");
}

void LogJITBailout(u32 pc, u32 opcode, const char* reasonName) {
    LogJIT("[JIT BAILOUT] PC: 0x%08X | Opcode: 0x%04X | Reason: %s\n", pc, opcode, reasonName);
}

void LogJITBlockCompileEnd(u32 startPC, u32 endPC, u32 instrCount, u32 staticCycles, bool bailedOut, u32 bailoutReason) {
    if (bailedOut) {
        LogJIT("=== BLOCK COMPILE FAILED @ 0x%08X (Reason Code: %u) ===\n", startPC, bailoutReason);
    } else {
        LogJIT("=== BLOCK COMPILED @ 0x%08X -> 0x%08X | Insns: %u | Cycles: %u ===\n", startPC, endPC, instrCount, staticCycles);
    }
}

void LogJITTraceExecution(bool isEntry, u32 entryPC, u32 nextPC, const u32 flags[4], u32 cycles) {
    if (jitStats.traceLogCount >= MAX_JIT_TRACE_CALLS) return;

    if (isEntry) {
       LogJIT("\n[JIT IN  #%2d] Entry PC: 0x%08X | Flags (N Z C V): %u %u %u %u\n",
    		   jitStats.traceLogCount, entryPC, flags[0], flags[1], flags[2], flags[3]);
    } else {
        LogJIT("[JIT OUT #%2d] Entry PC: 0x%08X -> NextPC: 0x%08X | Flags (N Z C V): %u %u %u %u | Cycles: %u\n\n",
        		jitStats.traceLogCount, entryPC, nextPC, flags[0], flags[1], flags[2], flags[3], cycles);
    	jitStats.traceLogCount++;
    }
}

// Flushes the accumulated string buffer out to the SD card log file
void WriteJITLogToFile() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char logPath[128];

    if (t != NULL) {
        strftime(logPath, sizeof(logPath), "sd:/jit-log-trace-%Y%m%d-%H%M%S.txt", t);
    } else {
        snprintf(logPath, sizeof(logPath), "sd:/jit-log-trace.txt");
    }

    FILE* logFile = fopen(logPath, "w");
    if (logFile != nullptr) {
        fprintf(logFile, "--- JIT LOG START ---\n\n");
        fputs(g_jitLogBuffer.c_str(), logFile);
        fprintf(logFile, "--- JIT LOG END ---\n");
        fclose(logFile);
    }

    // Clear buffer after writing
    g_jitLogBuffer.clear();
    g_jitLogBuffer.shrink_to_fit();
}
#endif

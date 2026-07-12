#include "JITDebugLog.h"

#include <time.h>
#include <ogc/system.h>
#include <string>
#include <cstdarg>
#include <cstdio>

// -----------------------------------------------------------------------------
// JIT Trace Logger Buffer & Utility Method
// -----------------------------------------------------------------------------
static std::string g_jitLogBuffer;
static int g_jitLogCount = 0;
static const int MAX_JIT_TRACE_CALLS = 50;

// Helper to handle va_list directly without double formatting/buffer corruption
static void vLogJITInternal(const char* format, va_list args) {
    char tempBuf[512];
    vsnprintf(tempBuf, sizeof(tempBuf), format, args);
    g_jitLogBuffer.append(tempBuf);
}

// Appends formatted log strings to the in-memory log buffer
void LogJIT(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vLogJITInternal(format, args);
    va_end(args);
}

void LogJITMismatch(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vLogJITInternal(format, args);
    va_end(args);
}

void LogJITBlockCompileStart(u32 startPC) {
	return;
    LogJIT("=== COMPILING JIT BLOCK @ 0x%08X ===\n", startPC);
}

void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...) {
	return;
    char details[256];
    va_list args;
    va_start(args, format);
    vsnprintf(details, sizeof(details), format, args);
    va_end(args);
	LogJIT("  [0x%08X] Opcode: 0x%04X | %s\n", pc, opcode, details);
}

void LogJITBailout(u32 pc, u32 opcode, const char* reasonName) {
	return;
    LogJIT("[JIT BAILOUT] PC: 0x%08X | Opcode: 0x%04X | Reason: %s\n", pc, opcode, reasonName);
}

void LogJITBlockCompileEnd(u32 startPC, u32 endPC, u32 instrCount, u32 staticCycles, bool bailedOut, u32 bailoutReason) {
    return;
    if (bailedOut) {
        LogJIT("=== BLOCK COMPILE FAILED @ 0x%08X (Reason Code: %u) ===\n", startPC, bailoutReason);
    } else {
        LogJIT("=== BLOCK COMPILED @ 0x%08X -> 0x%08X | Insns: %u | Cycles: %u ===\n", startPC, endPC, instrCount, staticCycles);
    }
}

void LogJITTraceExecution(bool isEntry, u32 entryPC, u32 nextPC, const u32 flags[4], u32 cycles) {
    if (g_jitLogCount >= MAX_JIT_TRACE_CALLS) return;

    if (isEntry) {
        LogJIT("\n[JIT IN  #%2d] Entry PC: 0x%08X | Flags (N Z C V): %u %u %u %u\n",
            g_jitLogCount, entryPC, flags[0], flags[1], flags[2], flags[3]);
    } else {
        LogJIT("[JIT OUT #%2d] Entry PC: 0x%08X -> NextPC: 0x%08X | Flags (N Z C V): %u %u %u %u | Cycles: %u\n\n",
            g_jitLogCount, entryPC, nextPC, flags[0], flags[1], flags[2], flags[3], cycles);
        g_jitLogCount++;
    }
}

// Flushes the accumulated string buffer out to the SD card log file
void WriteJITLogToFile() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char logPath[128];

    if (t != NULL) {
        strftime(logPath, sizeof(logPath), "sd:/jit-log-%Y%m%d-%H%M%S.txt", t);
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

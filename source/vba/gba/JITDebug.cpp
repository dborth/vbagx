#ifndef NO_JIT_COMPILER
#include "JIT.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "mem2.h"

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
static char*  jitLogBuffer   = NULL;
static size_t jitLogCapacity = 0;
static size_t jitLogSize     = 0;
static bool JITBlockDumped = false;

void InitJITLog() {
	if (!jitLogBuffer) {
		jitLogCapacity = 2 * 1024 * 1024;
		jitLogBuffer = (char*)mem2_malloc(jitLogCapacity);
	}
	
	jitLogSize = 0;
	if (jitLogBuffer) {
		jitLogBuffer[0] = '\0';
	}
	JITBlockDumped = false;
}

static void vLogJITInternal(const char* format, va_list args) {
	if (!jitLogBuffer || (jitLogCapacity - jitLogSize) <= 1) {
		return;
	}

	size_t remaining = jitLogCapacity - jitLogSize;
	int written = vsnprintf(jitLogBuffer + jitLogSize, remaining, format, args);

	if (written > 0) {
		if ((size_t)written >= remaining) {
			// Buffer filled completely up to the safe bounds limit
			jitLogSize = jitLogCapacity;
		} else {
			jitLogSize += written;
		}
	}
}

void LogJIT(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vLogJITInternal(format, args);
	va_end(args);
}

void LogJITMismatch(const char* message) {
	if (!jitLogBuffer || !message || (jitLogCapacity - jitLogSize) <= 1) {
		return;
	}

	jitStats.mismatchCount++;
	LogJIT("==================== [JIT DIFFERENTIAL MISMATCH #%d] ====================\n", jitStats.mismatchCount);
	LogJIT("%s", message);
	LogJIT("========================================================================\n");
}

void LogJITBlockCompileStart(u32 startPC) {
    LogJIT("=== COMPILING JIT BLOCK @ 0x%08X ===\n", startPC);
}

void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...) {
	if (!jitLogBuffer || (jitLogCapacity - jitLogSize) <= 1) {
		return;
	}

	LogJIT("  [0x%08X] Opcode: 0x%04X | ", pc, opcode);

	va_list args;
	va_start(args, format);
	vLogJITInternal(format, args);
	va_end(args);

	LogJIT("\n");
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
        fputs(jitLogBuffer, logFile);
        fprintf(logFile, "--- JIT LOG END ---\n");
        fclose(logFile);
    }

	// Clear buffer after writing
	if (jitLogBuffer) {
		mem2_free(jitLogBuffer);
		jitLogBuffer = NULL;
	}
	jitLogSize = 0;
	jitLogCapacity = 0;
}

// run with powerpc-eabi-objdump -D -b binary -m powerpc -EB jit-trace-dump.bin > dump.txt
void DebugDumpFirstJITBlock(BasicBlock* block) {
	if (__builtin_expect(block == nullptr || block->execute == nullptr || block->length == 0, 0)) {
		return;
	}

	if (__builtin_expect(!JITBlockDumped, 0)) {
		JITBlockDumped = true;

		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		char logPath[128];

		if (t != NULL) {
			strftime(logPath, sizeof(logPath), "sd:/jit-trace-dump-%Y%m%d-%H%M%S.bin", t);
		} else {
			snprintf(logPath, sizeof(logPath), "sd:/jit-trace-dump.bin");
		}

		FILE* dumpFile = fopen(logPath, "wb");
		if (dumpFile != nullptr) {
			// Allocate a generous threshold of native instructions per guest instruction
			size_t maxWords = block->length * 32;
			if (maxWords > 2048) maxWords = 2048; // Cap at 8KB to prevent memory bleed

			size_t dumpWords = maxWords;
			u32* nativeCode = (u32*)block->execute;

			// Scan code space for native PowerPC exit branches
			for (size_t i = 0; i < maxWords; i++) {
				// 0x4E800020 = PPC_BLR() | 0x4E800420 = PPC_BCTR()
				if (nativeCode[i] == 0x4E800020 || nativeCode[i] == 0x4E800420) {
					dumpWords = i + 1; // Snip right after the terminator instruction
					break;
				}
			}

			fwrite((u32*)block->execute, sizeof(u32), dumpWords, dumpFile);
			fclose(dumpFile);
		}
	}
}
#endif

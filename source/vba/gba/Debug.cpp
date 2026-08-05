/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * Debug.cpp
 *
 * Implements the logging/reporting functions declared in Debug.h and gated
 * behind VBAGX_DEBUG. Everything here writes into one shared in-memory
 * text buffer (debugLogBuffer, allocated from MEM2 via mem2_malloc) rather
 * than touching the SD card per call, since SD I/O during emulation would
 * be far too slow — WriteDebugLogToFile() is the one place that actually
 * flushes the accumulated buffer out to a timestamped file
 * (sd:/vbagx-debug-log-<timestamp>.txt) and frees it.
 *
 * Notable pieces:
 *   - InitDebugLog()/vLogDebugInternal()/LogDebug(): buffer lifecycle and
 *     the bounds-checked vsnprintf wrapper every other logging function
 *     funnels through.
 *   - LogJITMismatch(): formats and counts a differential-testing mismatch
 *     report (capped via debugStats.mismatchCount / MAX_JIT_MISMATCH_COUNT
 *     in JITDifferential.cpp, which builds the message this just appends).
 *   - LogJITBlockCompileStart/End, LogJITInsnCompiled, LogJITBailout: the
 *     detailed per-instruction/per-block compile-time trace text (only
 *     active under JIT_DETAILED_LOG).
 *   - LogJITTraceExecution(): entry/exit trace logging for each compiled
 *     block's execution, capped at MAX_JIT_TRACE_CALLS.
 *   - DebugDumpFirstJITBlock(): one-time raw dump of the first successfully
 *     compiled block's native PowerPC bytes to an SD-card .bin file,
 *     scanning forward for a blr/bctr terminator to avoid dumping garbage
 *     past the block's actual end — meant to be fed to
 *     `powerpc-eabi-objdump -D -b binary -m powerpc -EB` for inspection.
 ***************************************************************************/

#if VBAGX_DEBUG
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "JIT.h"
#include "memmanager.h"

// -----------------------------------------------------------------------------
// Debug Logger Buffer & Utility Method
// -----------------------------------------------------------------------------
static char*  debugLogBuffer   = NULL;
static size_t jitLogCapacity = 0;
static size_t jitLogSize     = 0;
static bool JITBlockDumped = false;

void InitDebugLog() {
	if (!debugLogBuffer) {
		jitLogCapacity = 2 * 1024 * 1024;
		debugLogBuffer = (char*)extmem_malloc(jitLogCapacity);
	}
	
	jitLogSize = 0;
	if (debugLogBuffer) {
		debugLogBuffer[0] = '\0';
	}
	JITBlockDumped = false;
}

static void vLogDebugInternal(const char* format, va_list args) {
	if (!debugLogBuffer || (jitLogCapacity - jitLogSize) <= 1) {
		return;
	}

	size_t remaining = jitLogCapacity - jitLogSize;
	int written = vsnprintf(debugLogBuffer + jitLogSize, remaining, format, args);

	if (written > 0) {
		if ((size_t)written >= remaining) {
			// Buffer filled completely up to the safe bounds limit
			jitLogSize = jitLogCapacity;
		} else {
			jitLogSize += written;
		}
	}
}

void LogDebug(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vLogDebugInternal(format, args);
	va_end(args);
}

void LogJITMismatch(const char* message) {
	if (!debugLogBuffer || !message || (jitLogCapacity - jitLogSize) <= 1) {
		return;
	}

	LogDebug("==================== [JIT DIFFERENTIAL MISMATCH #%d] ====================\n", debugStats.mismatchCount);
	LogDebug("%s", message);
	LogDebug("========================================================================\n");
}

void LogJITBlockCompileStart(u32 startPC) {
    LogDebug("=== COMPILING JIT BLOCK @ 0x%08X ===\n", startPC);
}

void LogJITInsnCompiled(u32 pc, u16 opcode, const char* format, ...) {
	if (!debugLogBuffer || (jitLogCapacity - jitLogSize) <= 1) {
		return;
	}

	LogDebug("  [0x%08X] Opcode: 0x%04X | ", pc, opcode);

	va_list args;
	va_start(args, format);
	vLogDebugInternal(format, args);
	va_end(args);

	LogDebug("\n");
}

void LogJITBailout(u32 pc, u32 opcode, const char* reasonName) {
    LogDebug("[JIT BAILOUT] PC: 0x%08X | Opcode: 0x%04X | Reason: %s\n", pc, opcode, reasonName);
}

void LogJITBlockCompileEnd(u32 startPC, u32 endPC, u32 instrCount, u32 staticCycles, bool bailedOut, u32 bailoutReason) {
    if (bailedOut) {
        LogDebug("=== BLOCK COMPILE FAILED @ 0x%08X (Reason Code: %u) ===\n", startPC, bailoutReason);
    } else {
        LogDebug("=== BLOCK COMPILED @ 0x%08X -> 0x%08X | Insns: %u | Cycles: %u ===\n", startPC, endPC, instrCount, staticCycles);
    }
}

void LogJITTraceExecution(bool isEntry, u32 entryPC, u32 nextPC, CPUFlags flags, u32 cycles) {
    if (debugStats.traceLogCount >= MAX_JIT_TRACE_CALLS) return;

    if (isEntry) {
       LogDebug("\n[JIT IN  #%2d] Entry PC: 0x%08X | Flags (N Z C V): %u %u %u %u\n",
    		   debugStats.traceLogCount, entryPC, flags.N, flags.Z, flags.C, flags.V);
    } else {
        LogDebug("[JIT OUT #%2d] Entry PC: 0x%08X -> NextPC: 0x%08X | Flags (N Z C V): %u %u %u %u | Cycles: %u\n\n",
        		debugStats.traceLogCount, entryPC, nextPC, flags.N, flags.Z, flags.C, flags.V, cycles);
    	debugStats.traceLogCount++;
    }
}

// Flushes the accumulated string buffer out to the SD card log file
void WriteDebugLogToFile() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char logPath[128];

    if (t != NULL) {
        strftime(logPath, sizeof(logPath), "sd:/vbagx-debug-log-%Y%m%d-%H%M%S.txt", t);
    } else {
        snprintf(logPath, sizeof(logPath), "sd:/vbagx-debug-log.txt");
    }

    FILE* logFile = fopen(logPath, "w");
    if (logFile != nullptr) {
        fprintf(logFile, "--- DEBUG LOG START ---\n\n");
        fputs(debugLogBuffer, logFile);
        fprintf(logFile, "--- DEBUG LOG END ---\n");
        fclose(logFile);
    }

	// Clear buffer after writing
	if (debugLogBuffer) {
		extmem_free(debugLogBuffer);
		debugLogBuffer = NULL;
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

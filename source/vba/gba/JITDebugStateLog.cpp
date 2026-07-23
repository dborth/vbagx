#ifndef NO_JIT_COMPILER
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "JITDebugStateLog.h"
#include "Globals.h"

#define DEBUG_STATE_LOG_BUFFER_SIZE		(2 * 1024 * 1024)
#define MAX_DEBUG_INSTRUCTIONS			5000

JITDebugStateLog jitDebugStateLog;

void JITDebugStateLog::Init(char * logType) {
	logBuffer = (char *)malloc(DEBUG_STATE_LOG_BUFFER_SIZE);
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	char logTime[30] = "";

	if (t != NULL) {
		strftime(logTime, sizeof(logTime), "-%Y%m%d-%H%M%S", t);
	}

	snprintf(logPath, sizeof(logPath), "sd:/jit-log-state-%s%s.txt", logType, logTime);
}

void JITDebugStateLog::LogState(const char* source, u32 executedPC, u32 nextPC, u32 ticks, u32 cycles, u32 instrCount) {
	if(!logBuffer) {
		return;
	}

	if (instrCount >= MAX_DEBUG_INSTRUCTIONS) {
		WriteToFile();
		return;
	}

	int written = snprintf(logBuffer + currentOffset, DEBUG_STATE_LOG_BUFFER_SIZE - currentOffset,
			"%s Ticks: %08u | Cycles: %04d | InstrCount: %04d | PC: 0x%08X | NextPC: 0x%08X | "
		"Flags: N%d Z%d C%d V%d | "
		"R0: 0x%08X | R1: 0x%08X | R2: 0x%08X | R3: 0x%08X | "
		"R4: 0x%08X | R5: 0x%08X | R6: 0x%08X | R7: 0x%08X | "
		"R8: 0x%08X | R9: 0x%08X | R10: 0x%08X | R11: 0x%08X | "
		"R12: 0x%08X | SP: 0x%08X | LR: 0x%08X\n",
		source, ticks, cycles, instrCount, executedPC, nextPC,
		(reg[16].B.B0 >> 3) & 1,  // N Flag (assuming mapping matches your architecture)
		(reg[16].B.B0 >> 2) & 1,  // Z Flag
		(reg[16].B.B0 >> 1) & 1,  // C Flag
		(reg[16].B.B0 >> 0) & 1,  // V Flag
		reg[0].I, reg[1].I, reg[2].I, reg[3].I,
		reg[4].I, reg[5].I, reg[6].I, reg[7].I,
		reg[8].I, reg[9].I, reg[10].I, reg[11].I,
		reg[12].I, reg[13].I, reg[14].I
	);

	if (written > 0) {
		currentOffset += written;
	}
}

void JITDebugStateLog::WriteToFile() {
	if(!logBuffer) {
		return;
	}

	FILE* logFile = fopen(logPath, "wb");
	if (logFile) {
		fwrite(logBuffer, 1, currentOffset, logFile);
		fflush(logFile);
		fclose(logFile);
		logFile = nullptr;
	}

	free(logBuffer);
	logBuffer = nullptr;
}
#endif

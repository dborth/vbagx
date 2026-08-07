/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * cheatmgr.cpp
 *
 * Cheat manager -  Libretro .cht support for VBA-M
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "vbagx.h"
#include "vbasupport.h"
#include "memmanager.h"
#include "cheatmgr.h"
#include "fileop.h"
#include "filebrowser.h"
#include "vba/gba/Globals.h"
#include "vba/gba/Cheats.h"
#include "vba/gb/gbCheats.h"

static char* stringPool = NULL; // Contiguous memory block for all strings
Cheat cheats[MAX_CHEATS];
int cheatCount = 0;

/****************************************************************************
 * ToggleCheat
 *
 * Registers codes into VBA-M on enable, and deletes them on disable.
 ***************************************************************************/
static inline char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (c - 32) : c;
}

void ToggleCheat(int id) {
	if (id < 0 || id >= cheatCount)
		return;

	Cheat* group = &cheats[id];
	group->enabled = !group->enabled;

	if (group->enabled) {
		cheatsEnabled = true;

		// Toggle on - Register codes into VBA-M
		// Separate GBA and GB core counters
		group->vbaStartIndex = (cartridgeType == CARTRIDGE_GBA) ? cheatsNumber : gbCheatNumber;
		group->vbaCodeCount = 0;

		// Duplicate code string temporarily because strtok mutates in place
		char* codeCopy = strdup(group->rawCode);
		if (!codeCopy) return;

		char* token = strtok(codeCopy, "+");
		while (token != NULL) {
			// Sanitize string (strip spaces/hyphens and count hex chars)
			char sanitized[32] = {0};
			int len = 0;
			for (int i = 0; token[i] != '\0' && len < 31; i++) {
				if (token[i] != ' ' && token[i] != '-') {
					sanitized[len++] = to_upper(token[i]);
				}
			}
			sanitized[len] = '\0';

			if (cartridgeType == CARTRIDGE_GBA) {
				if (cheatsNumber >= 100) break; // Core limit safety check

				if (len == 12) {
					// CodeBreaker GBA expects a space inserted at index 8 (XXXXXXXX YYYY)
					char formatted[14];
					strncpy(formatted, sanitized, 8);
					formatted[8] = ' ';
					strncpy(formatted + 9, sanitized + 8, 4);
					formatted[13] = '\0';

					cheatsAddCBACode(formatted, group->name);
					cheatsEnable(cheatsNumber - 1);
					group->vbaCodeCount++;
				}
				else if (len == 16) {
					// GameShark GBA v3
					cheatsAddGSACode(sanitized, group->name, true);
					cheatsEnable(cheatsNumber - 1);
					group->vbaCodeCount++;
				}
			}
			else if (cartridgeType == CARTRIDGE_GB) {
				if (gbCheatNumber >= 100) break; // Core limit safety check

				if (len == 7 || len == 11) {
					// Game Boy Game Genie
					gbAddGgCheat(sanitized, group->name);
					gbCheatEnable(gbCheatNumber - 1);
					group->vbaCodeCount++;
				}
				else if (len == 8) {
					// Game Boy GameShark
					gbAddGsCheat(sanitized, group->name);
					gbCheatEnable(gbCheatNumber - 1);
					group->vbaCodeCount++;
				}
			}

			token = strtok(NULL, "+");
		}
		free(codeCopy);

	} else {
		// Toggle off - Remove codes completely from VBA-M
		for (int i = group->vbaCodeCount - 1; i >= 0; i--) {
			int targetIndex = group->vbaStartIndex + i;

			if (cartridgeType == CARTRIDGE_GBA) {
				cheatsDelete(targetIndex, false);
			} else if (cartridgeType == CARTRIDGE_GB) {
				gbCheatRemove(targetIndex);
			}
		}

		// Adjust starting indices for other active groups
		for (int g = 0; g < cheatCount; g++) {
			if (cheats[g].enabled && cheats[g].vbaStartIndex > group->vbaStartIndex) {
				cheats[g].vbaStartIndex -= group->vbaCodeCount;
			}
		}

		group->vbaStartIndex = -1;
		group->vbaCodeCount = 0;

		if (cartridgeType == CARTRIDGE_GBA && cheatsNumber == 0) {
			cheatsEnabled = false;
		} else if (cartridgeType == CARTRIDGE_GB && gbCheatNumber == 0) {
			cheatsEnabled = false;
		}
	}
}

void ResetCheats() {
	cheatsEnabled = false;
	cheatsDeleteAll(false);
	gbCheatRemoveAll();
	cheatCount = 0;
	memset(cheats, 0, sizeof(cheats));

	if (stringPool) {
		free(stringPool);
		stringPool = NULL;
	}
}

/****************************************************************************
 * LoadCheatFile
 *
 * Erases any pre-existing cheats, loads cheats from a cheat file
 * Called when a ROM is first loaded
 ***************************************************************************/

// Helper: Safely get the next line without modifying the buffer
static const char* GetNextLine(const char* cursor) {
	if (!cursor || !*cursor) return NULL;
	// Skip to end of current line
	while (*cursor && *cursor != '\n' && *cursor != '\r') cursor++;
	// Skip line break characters to reach next line (handles \r, \n, \r\n, \n\r)
	while (*cursor == '\n' || *cursor == '\r') cursor++;
	return (*cursor) ? cursor : NULL;
}

// Helper: Measure or Extract Quoted String securely within a single line
static int ProcessQuotedString(const char* line, char* dst, int maxLen) {
	if (!line) return 0;

	// 1. Constrain search strictly to the current line
	const char* nl = strchr(line, '\n');
	const char* cr = strchr(line, '\r');
	const char* end = nl;
	if (cr && (!nl || cr < nl)) end = cr;
	if (!end) end = line + strlen(line);

	// 2. Find the first quote
	const char* q1 = line;
	while (q1 < end && *q1 != '"') q1++;

	// 3. Fallback: If no quotes exist, find '=' and extract the trimmed string
	if (q1 >= end) {
		const char* eq = line;
		while (eq < end && *eq != '=') eq++;
		if (eq >= end) {
			if (dst && maxLen > 0) dst[0] = '\0';
			return 0;
		}
		const char* start = eq + 1;
		while (start < end && (*start == ' ' || *start == '\t')) start++;
		const char* tail = end - 1;
		while (tail >= start && (*tail == ' ' || *tail == '\t')) tail--;

		int len = tail - start + 1;
		if (len <= 0) {
			if (dst && maxLen > 0) dst[0] = '\0';
			return 0;
		}
		if (dst && maxLen > 0) {
			int copyLen = len >= maxLen ? maxLen - 1 : len;
			strncpy(dst, start, copyLen);
			dst[copyLen] = '\0';
		}
		return len;
	}

	// 4. Find the last quote on this line (supports internal quotes)
	const char* q2 = end - 1;
	while (q2 > q1 && *q2 != '"') q2--;

	if (q2 <= q1) {
		if (dst && maxLen > 0) dst[0] = '\0';
		return 0;
	}

	int len = q2 - q1 - 1;
	if (dst && maxLen > 0) {
		int copyLen = len >= maxLen ? maxLen - 1 : len;
		strncpy(dst, q1 + 1, copyLen);
		dst[copyLen] = '\0';
	}
	return len; // Always return true required length for pool allocation
}

struct TempCheatInfo {
	int descLen;
	int codeLen;
	char desc[256];
	char code[1024];
};

// Helper: Validates and sanitizes cheat strings according to format rules
static void SanitizeCheatString(const char* raw, char* outBuffer, int maxOutLen) {
	outBuffer[0] = '\0';
	int outLen = 0;
	char acc[64] = {0};
	int accLen = 0;

	// Read character by character to safely extract hex blocks and drop bad data
	for (int i = 0; ; i++) {
		char c = raw[i];

		// 1. Accumulate valid hex characters
		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
			if (accLen < 63) {
				acc[accLen++] = to_upper(c);
				acc[accLen] = '\0';
			}
		}
		// 2. Evaluate tokens when hitting a separator or end-of-string
		else if (c == '+' || c == ' ' || c == '-' || c == '\0') {
			if (cartridgeType == CARTRIDGE_GBA) {
				if (accLen == 12) {
					// CodeBreaker GBA: Formatted strictly as XXXXXXXX YYYY
					if (outLen + 14 < maxOutLen) {
						if (outLen > 0) { outBuffer[outLen++] = '+'; }
						strncpy(outBuffer + outLen, acc, 8);
						outLen += 8;
						outBuffer[outLen++] = ' ';
						strncpy(outBuffer + outLen, acc + 8, 4);
						outLen += 4;
						outBuffer[outLen] = '\0';
					}
					accLen = 0; // Reset for next token
				} else if (accLen == 16) {
					// GameShark GBA v3: Formatted as XXXXXXXXXXXXXXXX
					if (outLen + 17 < maxOutLen) {
						if (outLen > 0) { outBuffer[outLen++] = '+'; }
						strcpy(outBuffer + outLen, acc);
						outLen += 16;
					}
					accLen = 0;
				} else if (c == '\0') {
					accLen = 0; // End of string, discard any incomplete accumulation
				}
			}
			else if (cartridgeType == CARTRIDGE_GB) {
				// Game Boy Game Genie (7, 11) or Game Boy GameShark (8)
				if (accLen == 7 || accLen == 8 || accLen == 11) {
					if (outLen + accLen + 2 < maxOutLen) {
						if (outLen > 0) { outBuffer[outLen++] = '+'; }
						strcpy(outBuffer + outLen, acc);
						outLen += accLen;
					}
					accLen = 0;
				} else if (c == '\0') {
					accLen = 0;
				}
			}
		}
		// 3. Drop bad data: Clear accumulator if an invalid character is hit
		else {
			accLen = 0;
			acc[0] = '\0';
		}

		if (c == '\0') {
			break;
		}
	}
}

void LoadCheatFile() {
	char filepath[1024];
	int fileSize = 0;

	if(!MakeFilePath(filepath, FILE_CHEAT))
		return;

	AllocSaveBuffer();

	fileSize = LoadFile(filepath, SILENT);

	if(fileSize == 0) {
		FreeSaveBuffer();
		return;
	}

	TempCheatInfo* tempCheats = (TempCheatInfo*)mem1_malloc(MAX_CHEATS * sizeof(TempCheatInfo));

	if (!tempCheats) {
		FreeSaveBuffer();
		return;
	}

	memset(tempCheats, 0, MAX_CHEATS * sizeof(TempCheatInfo));

	int maxIdx = -1;
	const char* cursor = (const char*)savebuffer;

	// Parse the entire file safely regardless of line order
	while (cursor) {
		int curIdx = -1;
		char key[32] = {0};

		if (sscanf(cursor, " cheat%d_%31[^= \t]", &curIdx, key) == 2) {
			if (curIdx >= 0 && curIdx < MAX_CHEATS) {
				if (curIdx > maxIdx) {
					maxIdx = curIdx;
				}

				if (strcmp(key, "desc") == 0) {
					tempCheats[curIdx].descLen = ProcessQuotedString(cursor, tempCheats[curIdx].desc, sizeof(tempCheats[curIdx].desc));
				}
				else if (strcmp(key, "code") == 0) {
					char rawCode[1024] = {0};
					ProcessQuotedString(cursor, rawCode, sizeof(rawCode));

					// Safely construct a guaranteed perfectly formatted code string
					SanitizeCheatString(rawCode, tempCheats[curIdx].code, sizeof(tempCheats[curIdx].code));
					tempCheats[curIdx].codeLen = strlen(tempCheats[curIdx].code);
				}
			}
		}

		cursor = GetNextLine(cursor);
	}

	// Calculate the necessary contiguous string pool size
	int poolSize = 0;
	for (int i = 0; i <= maxIdx; i++) {
		if (tempCheats[i].codeLen > 0) { // Only count valid, actionable cheats
			poolSize += (tempCheats[i].descLen > 0 ? tempCheats[i].descLen : 13) + 1;
			poolSize += tempCheats[i].codeLen + 1;
		}
	}

	if (poolSize == 0) {
		mem1_free(tempCheats);
		FreeSaveBuffer();
		return;
	}

	stringPool = (char*)malloc(poolSize);
	if (!stringPool) {
		mem1_free(tempCheats);
		FreeSaveBuffer();
		return;
	}

	char* poolCursor = stringPool;
	cheatCount = 0;

	// Transfer assembled temporary objects into the actual cheats array
	for (int i = 0; i <= maxIdx && cheatCount < MAX_CHEATS; i++) {
		if (tempCheats[i].codeLen > 0) {
			Cheat* g = &cheats[cheatCount++];

			const char* nameStr = (tempCheats[i].descLen > 0) ? tempCheats[i].desc : "Unnamed Cheat";
			g->name = poolCursor;
			poolCursor += sprintf(poolCursor, "%s", nameStr) + 1;

			g->rawCode = poolCursor;
			poolCursor += sprintf(poolCursor, "%s", tempCheats[i].code) + 1;

			g->enabled = false;
			g->vbaStartIndex = -1;
			g->vbaCodeCount = 0;
		}
	}

	mem1_free(tempCheats);
	FreeSaveBuffer();
}

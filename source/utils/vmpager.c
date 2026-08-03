/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 *
 * vmpager.c
 *
 * GameBoy Advance Hardware Virtual Memory Paging (ARAM MMU)
 ***************************************************************************/
#ifdef HW_DOL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogc/aram.h>
#include <ogc/cache.h>
#include <ogc/message.h>
#include <ogc/lwp.h>
#include <ogc/system.h>

#include "vmpager.h"
#include "vm.h"

extern int GBAROMSize;
extern u8 *rom;

static FILE* romfile = NULL;

void VMPager_Init() {
	
}

void VMPager_Shutdown() {
	
}

int VMPager_LoadROM(const char * filepath) {
	VMPager_CloseFile();
	
	romfile = fopen(filepath, "rb");
	if (romfile == NULL) {
		return 0;
	}

	fseeko(romfile, 0, SEEK_END);
	GBAROMSize = ftello(romfile);
	fseeko(romfile, 0, SEEK_SET);

	rom = (u8*)VM_GetBase();

	const u32 CHUNK_SIZE = 64 * 1024;
	u8* chunkBuffer = (u8*)memalign(32, CHUNK_SIZE);
	u32 offset = 0;

	while (offset < GBAROMSize) {
		u32 readSize = GBAROMSize - offset;
		if (readSize > CHUNK_SIZE) readSize = CHUNK_SIZE;

		fread(chunkBuffer, 1, readSize, romfile);
		memcpy(rom + offset, chunkBuffer, readSize);
		offset += readSize;
	}

	free(chunkBuffer);
	fclose(romfile);

	return GBAROMSize;
}

void VMPager_CloseFile() {
	if (romfile) {
		fclose(romfile);
		romfile = NULL;
	}
	rom = NULL;
	VM_Clear();
}
#endif

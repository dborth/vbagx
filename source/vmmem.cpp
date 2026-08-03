/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2008-2026
 *
 * vmmem.cpp
 *
 * GameBoy Advance Hardware Virtual Memory Paging (ARAM MMU)
 ***************************************************************************/
#ifdef HW_DOL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogc/aram.h>

#include "vbagx.h"
#include "vmmem.h"
#include "vbasupport.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu.h"
#include "vba/Util.h"
#include "vba/gba/GBA.h"
#include "vba/gba/Globals.h"

extern "C" {
#include "utils/vm.h"
}

int VMGBAROMLoad()
{
	char filepath[MAXPATHLEN];
	if(!MakeFilePath(filepath, FILE_ROM))
		return 0;

	if(!utilIsGBAImage(filepath)) {
		ErrorPrompt("Compressed GBA files are not supported!");
		return 0;
	}

	FILE* romfile = fopen(filepath, "rb");
	if (romfile == NULL) {
		ErrorPrompt("Error opening file!");
		return 0;
	}

	// Get exact ROM size
	fseeko(romfile, 0, SEEK_END);
	GBAROMSize = ftello(romfile);
	fseeko(romfile, 0, SEEK_SET);
	
	VMClose();

	if(!GBAROMAlloc()) {
		fclose(romfile);
		return 0;
	}

	rom = (u8*)VM_GetBase();
	ShowAction("Loading...");

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

void VMClose()
{
	rom = NULL;
}
#endif

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

#include "vbagx.h"
#include "vbasupport.h"
#include "filebrowser.h"
#include "menu.h"
#include "vba/Util.h"
#include "vba/gba/Globals.h"

extern "C" {
#include "utils/vmpager.h"
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

	ShowAction("Loading...");
    int size = VMPager_LoadROM(filepath);
    CancelAction();

    if(size == 0) {
    	ErrorPrompt("Error opening file!");
    }
    
    if(!GBAROMAlloc()) {
		return 0;
	}

    return size;
}

void VMClose()
{
	VMPager_CloseFile();
}
#endif

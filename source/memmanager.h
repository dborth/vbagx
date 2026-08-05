/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * memmanager.h
 *
 * Memory manager
 ***************************************************************************/

#ifndef _MEMMANAGER_H_
#define _MEMMANAGER_H_

#include <gctypes.h>
#include "filebrowser.h"
#include "fileop.h"
#include "vba/gba/JITCache.h"

#define IMAGE_BUFFER_SIZE (640 * 480 * 4)

// Mode 1: Menu
struct MenuMemory {
    BROWSERENTRY browserList[MAX_BROWSER_SIZE];
    u8 savebuffer[SAVEBUFFERSIZE];
    u8 imageBuffer[IMAGE_BUFFER_SIZE];
    u8 gameScreen[IMAGE_BUFFER_SIZE];
    u8 workBuffer1[IMAGE_BUFFER_SIZE];
    u8 workBuffer2[IMAGE_BUFFER_SIZE];
} __attribute__((aligned(32)));

// Mode 2: GB Game
struct GBMemory {

} __attribute__((aligned(32)));

// Mode 3: GBA Game
struct GBAMemory {
    u32 jitArena[JIT_ARENA_SIZE / sizeof(u32)];
    u8 blockTable[HASH_TABLE_SIZE * 16];
    u8 smcPageFlags[SMC_MAP_SIZE];
    u8 smcRegistry[SMC_MAP_SIZE * sizeof(void*)];
} __attribute__((aligned(32)));

// The Master Overlay
union CoreMemoryOverlay {
    struct MenuMemory menu;
    struct GBMemory gb;
    struct GBAMemory gba;
};

extern union CoreMemoryOverlay coreMem;
extern u8* romPtr;

void InitMemManager();
void SwitchMemoryModeMenu();
void SwitchMemoryModeGame();

void* extmem_malloc(u32 size);
void extmem_free(void *ptr);
int extmem_size_free();

#endif

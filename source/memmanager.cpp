/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * memmanager.cpp
 *
 * Memory manager
 ***************************************************************************/

#include <ogc/lwp_heap.h>
#include <ogc/system.h>
#include "vbagx.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "fileop.h"
#include "vba/gba/Globals.h"

#ifdef HW_DOL
#include "utils/vm.h"
#include "utils/vmpager.h"
#endif

#define MEM2_SIZE		(42*1024*1024)

enum
{
	MEMORY_MODE_MENU,
	MEMORY_MODE_GAME
};


static heap_cntrl extmem_heap;
static int memoryMode = -1;
static mutex_t sharedBufferLock = LWP_MUTEX_NULL;
static unsigned char *sharedBuffer = NULL;

void InitMemManager ()
{
#ifdef HW_RVL
	void *mem2_heap_ptr = SYS_AllocArenaMem2Hi(MEM2_SIZE, 32);
	__lwp_heap_init(&extmem_heap, mem2_heap_ptr, MEM2_SIZE, 32);
	sharedBuffer = (unsigned char *)extmem_malloc(SHAREDBUFFERSIZE);
	rom = (u8 *)extmem_malloc(MAX_GBA_ROM_SIZE); // allocate 32 MB to GBA ROM
#else
	VM_Init(MAX_GBA_ROM_SIZE, 2 * 1024 * 1024); // 2MB MEM1 + 16 ARAM + SD backing for GBA ROM
    VMPager_Init();
    sharedBuffer = (unsigned char *)memalign(32,SHAREDBUFFERSIZE);
#endif

	LWP_MutexInit(&sharedBufferLock, false);
}

unsigned char * getSharedBuffer()
{
	LWP_MutexLock(sharedBufferLock);
	return sharedBuffer;
}

void ReleaseSharedBuffer()
{
	LWP_MutexUnlock(sharedBufferLock);
}

void* extmem_malloc(u32 size)
{
	return __lwp_heap_allocate(&extmem_heap, size);
}

void extmem_free(void *ptr)
{
	__lwp_heap_free(&extmem_heap, ptr);
}

int extmem_size_free()
{
	heap_iblock info;
	__lwp_heap_getinfo(&extmem_heap,&info);
	return info.free_size;
}

void SwitchMemoryModeMenu() {
	if(memoryMode == MEMORY_MODE_MENU)
		return;

	memoryMode = MEMORY_MODE_MENU;

#ifdef HW_RVL
	browserList = (BROWSERENTRY *)extmem_malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#else
	browserList = (BROWSERENTRY *)malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#endif
}

void SwitchMemoryModeGame() {
	if(memoryMode == MEMORY_MODE_GAME)
		return;

	memoryMode = MEMORY_MODE_GAME;
	extmem_free(browserList);
	browserList = NULL;
}

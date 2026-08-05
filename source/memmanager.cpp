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
#include "vbasupport.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "fileop.h"

#ifdef HW_DOL
#include "utils/vm.h"
#include "utils/vmpager.h"
#endif

#define MEM2_SIZE		(42*1024*1024)

enum {
	MEMORY_MODE_NONE = -1,
	MEMORY_MODE_MENU = 0,
	MEMORY_MODE_GB,
	MEMORY_MODE_GBA
};

alignas(32) union CoreMemoryOverlay coreMem;
u8 *romPtr;
static heap_cntrl mem2_heap;
static int memoryMode = -1;

void InitMemManager ()
{
#ifdef HW_RVL
	void *mem2_heap_ptr = SYS_AllocArenaMem2Hi(MEM2_SIZE, 32);
	__lwp_heap_init(&mem2_heap, mem2_heap_ptr, MEM2_SIZE, 32);
	romPtr = (u8 *)extmem_malloc(MAX_GBA_ROM_SIZE); // allocate 32 MB to GBA ROM
#else
	romPtr = (u8 *)VM_Init(MAX_GBA_ROM_SIZE, 2 * 1024 * 1024); // 2MB MEM1 + 16 ARAM + SD backing for GB/GBA ROM
	VMPager_Init(romPtr);
#endif
}

void* extmem_malloc(u32 size)
{
	return __lwp_heap_allocate(&mem2_heap, size);
}

void extmem_free(void *ptr)
{
	__lwp_heap_free(&mem2_heap, ptr);
}

int extmem_size_free()
{
	heap_iblock info;
	__lwp_heap_getinfo(&mem2_heap,&info);
	return info.free_size;
}

static bool ChangeMode(int mode) {
	if(memoryMode == mode)
		return false;

	browserList = NULL;
	savebuffer = NULL;
	jitCache.destroy();
	memoryMode = mode;
	return true;
}

void SwitchMemoryModeMenu() {
	if(!ChangeMode(MEMORY_MODE_MENU)) return;
	browserList = coreMem.menu.browserList;
	savebuffer = coreMem.menu.savebuffer;
}

static void SwitchMemoryModeGB() {
	if(!ChangeMode(MEMORY_MODE_GB)) return;
}

static void SwitchMemoryModeGBA() {
	if(!ChangeMode(MEMORY_MODE_GBA)) return;

	jitCache.initialize(
		(u32*)coreMem.gba.jitArena,
		(BasicBlock*)coreMem.gba.blockTable,
		(BasicBlock**)coreMem.gba.smcRegistry,
		(u8*)coreMem.gba.smcPageFlags
	);
}

void SwitchMemoryModeGame() {
	if(IsGBAGame()) {
		SwitchMemoryModeGBA();
	}
	else {
		SwitchMemoryModeGB();
	}
}

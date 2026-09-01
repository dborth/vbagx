/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * memmanager.cpp
 *
 * Memory manager
 ***************************************************************************/

#include <ogc/system.h>
#include <malloc.h>
#include "vbagx.h"
#include "vbasupport.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "fileop.h"
#include "video.h"
#include "vba/gba/JITCache.h"

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

// Mode 3: GBA Game
struct GBAMemory {
    uint8_t texturemem[TEXTUREMEM_SIZE];
    uint32_t jitArena[JIT_ARENA_SIZE / sizeof(uint32_t)];
    uint8_t blockTable[HASH_TABLE_SIZE * 16];
    uint8_t smcPageFlags[SMC_MAP_SIZE];
    uint8_t smcRegistry[SMC_MAP_SIZE * sizeof(void*)];
} __attribute__((aligned(32)));

// Mode 2: GB Game
struct GBMemory {
	uint8_t texturemem[TEXTUREMEM_SIZE];
	uint8_t heapSpace[sizeof(struct GBAMemory) - TEXTUREMEM_SIZE];
} __attribute__((aligned(32)));

// Mode 1: Menu
struct MenuMemory {
    BROWSERENTRY browserList[MAX_BROWSER_SIZE];
    uint8_t heapSpace[sizeof(struct GBAMemory) - (sizeof(BROWSERENTRY) * MAX_BROWSER_SIZE)];
} __attribute__((aligned(32)));

// The Master Overlay
union CoreMemoryOverlay {
    struct MenuMemory menu;
    struct GBMemory gb;
    struct GBAMemory gba;
};

alignas(32) union CoreMemoryOverlay coreMem;
uint8_t *romPtr;
static mspace mem1_space = nullptr;
static mspace extmem_space = nullptr;
static int memoryMode = -1;

void InitMemManager ()
{
#ifdef HW_RVL
	void *mem2_heap_ptr = SYS_AllocArenaMem2Hi(MEM2_SIZE, 32);
	extmem_space = create_mspace_with_base(mem2_heap_ptr, MEM2_SIZE, 0);
	mspace_set_footprint_limit(extmem_space, MEM2_SIZE);
	romPtr = (uint8_t *)extmem_malloc(MAX_GBA_ROM_SIZE); // allocate 32 MB to GBA ROM
#else
	romPtr = (uint8_t *)VM_Init(MAX_GBA_ROM_SIZE, 2 * 1024 * 1024); // 2MB MEM1 + 16 ARAM + SD backing for GB/GBA ROM
	VMPager_Init(romPtr);
#endif
}

void* mem1_malloc(uint32_t size)
{
	if(!mem1_space) return nullptr;
	return mspace_malloc(mem1_space, size);
}

char* mem1_strdup(const char *s)
{
    if (!mem1_space || !s)
        return nullptr;

    size_t len = strlen(s) + 1;
    char *dup = (char *)mem1_malloc(len);

    if (dup)
        memcpy(dup, s, len);

    return dup;
}

void mem1_free(void *ptr)
{
	if(!mem1_space || !ptr) return;
	mspace_free(mem1_space, ptr);
}

int mem1_size_free()
{
	if(!mem1_space) return 0;
	struct mallinfo info = mspace_mallinfo(mem1_space);
	return info.fordblks;
}

void* extmem_malloc(uint32_t size)
{
	return mspace_malloc(extmem_space, size);
}

void extmem_free(void *ptr)
{
	mspace_free(extmem_space, ptr);
}

int extmem_size_free()
{
	if(!extmem_space) return 0;
	struct mallinfo info = mspace_mallinfo(extmem_space);
	return info.fordblks;
}

static bool ChangeMode(int mode) {
	if(memoryMode == mode)
		return false;

	MutexLock scratchGuard(GuiImageData::scratchLock());

	GuiImageData::setDecodeScratch(nullptr, 0);

	browserList = nullptr;
	savebuffer = nullptr;
	if(mem1_space) destroy_mspace(mem1_space);
	mem1_space = nullptr;
	texturemem = nullptr;
	jitCache.destroy();
	memoryMode = mode;
	return true;
}

static void CreateMem1Space(uint8_t *heapSpace, uint32_t size) {
	mem1_space = create_mspace_with_base(heapSpace, size, 0);
	mspace_set_footprint_limit(mem1_space, size);
	savebuffer = (uint8_t *)mem1_malloc(SAVEBUFFERSIZE);
}

void SwitchMemoryModeMenu() {
	if(!ChangeMode(MEMORY_MODE_MENU)) return;
	browserList = coreMem.menu.browserList;
	CreateMem1Space(coreMem.menu.heapSpace, sizeof(coreMem.menu.heapSpace));

	MutexLock scratchGuard(GuiImageData::scratchLock());
	void * decodeScratch = mem1_malloc(IMAGE_DECODE_SCRATCH_SIZE);
	GuiImageData::setDecodeScratch(decodeScratch, decodeScratch ? IMAGE_DECODE_SCRATCH_SIZE : 0);
}

static void SwitchMemoryModeGB() {
	if(!ChangeMode(MEMORY_MODE_GB)) return;
	texturemem = coreMem.gb.texturemem;
	CreateMem1Space(coreMem.gb.heapSpace, sizeof(coreMem.gb.heapSpace));
}

static void SwitchMemoryModeGBA() {
	if(!ChangeMode(MEMORY_MODE_GBA)) return;
	texturemem = coreMem.gba.texturemem;
	jitCache.initialize(
		(uint32_t*)coreMem.gba.jitArena,
		(BasicBlock*)coreMem.gba.blockTable,
		(BasicBlock**)coreMem.gba.smcRegistry,
		(uint8_t*)coreMem.gba.smcPageFlags
	);
}

void SwitchMemoryModeGame() {
	if(IsGBAGame()) {
		SwitchMemoryModeGBA();
	}
	else {
		SwitchMemoryModeGB();
	}
	memset(texturemem, 0, TEXTUREMEM_SIZE);
}

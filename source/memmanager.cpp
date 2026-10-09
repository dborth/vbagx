/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * memmanager.cpp
 *
 * Memory manager
 ***************************************************************************/

#ifdef GEKKO
#include <ogc/cache.h>
#include <ogc/system.h>
#endif
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vbagx.h"
#include "vbasupport.h"
#include "memmanager.h"
#include "filebrowser.h"
#include "fileop.h"
#include "video.h"
#include "vba/gba/JITCache.h"
#include "drivers/ThreadDriver.h"
#include "libgui/GuiImageData.h"

#ifdef HW_DOL
#include "drivers/ogc/gamecube/vm/vm.h"
#include "drivers/ogc/gamecube/vm/vmpager.h"
#endif

#ifdef __WIIU__
#include "drivers/wut/WutCodegen.h"
#endif

enum {
	MEMORY_MODE_NONE = -1,
	MEMORY_MODE_MENU = 0,
	MEMORY_MODE_GB,
	MEMORY_MODE_GBA
};

#ifdef HW_RVL
// Mode 3: GBA Game
struct GBAMemory {
    uint32_t jitArena[JIT_ARENA_SIZE / sizeof(uint32_t)];
} __attribute__((aligned(32)));

// Mode 2: GB Game
struct GBMemory {
	uint8_t heapSpace[sizeof(struct GBAMemory)];
} __attribute__((aligned(32)));
#else
// Mode 3: GBA Game
struct GBAMemory {
    uint8_t texturemem[TEXTUREMEM_SIZE];
    uint32_t jitArena[JIT_ARENA_SIZE / sizeof(uint32_t)];
    uint8_t blockTable[HASH_TABLE_SIZE * sizeof(BasicBlock)];
    uint8_t smcPageFlags[SMC_MAP_SIZE];
    uint8_t smcRegistry[SMC_MAP_SIZE * sizeof(void*)];
} __attribute__((aligned(32)));

// Mode 2: GB Game
struct GBMemory {
	uint8_t texturemem[TEXTUREMEM_SIZE];
	uint8_t heapSpace[sizeof(struct GBAMemory) - TEXTUREMEM_SIZE];
} __attribute__((aligned(32)));
#endif

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

#ifdef HW_RVL
// The core overlay is not a C++ object: it is a block of MEM1 reserved by the linker (wii_mem.ld) that
// first holds the embedded assets' load image. AssetsRelocate() moves the assets to MEM2, after which
// the whole block is the overlay (JIT arena / menu heap / GB heap) - no MEM1 is stranded.
extern "C" {
	extern char __assets_mem2_start[], __assets_mem2_end[]; // assets' run address (MEM2)
	extern char __assets_lma_start[];                       // assets' load address (MEM1)
	extern char __jit_region_start[], __jit_region_end[];   // the MEM1 block
	// keep libogc2's MEM2 arena below the asset image
	void *__myArena2Hi = __assets_mem2_start;
}

static union CoreMemoryOverlay &coreMem = *reinterpret_cast<union CoreMemoryOverlay*>(__jit_region_start);

// Must run before any embedded asset is used and before coreMem is used.
static void AssetsRelocate()
{
	// linker/code must agree on the block size (Makefile.wii JIT_ARENA_MB feeds both)
	if((size_t)(__jit_region_end - __jit_region_start) != sizeof(union CoreMemoryOverlay) ||
	   ((uintptr_t)__jit_region_start & 31))
		abort();

	memcpy(__assets_mem2_start, __assets_lma_start, __assets_mem2_end - __assets_mem2_start);
	DCFlushRange(__assets_mem2_start, __assets_mem2_end - __assets_mem2_start);
	// The block used to be .bss, which crt0 zeroed. Keep that contract: start from zeros.
	memset(__jit_region_start, 0, __jit_region_end - __jit_region_start);
}
#else
alignas(32) union CoreMemoryOverlay coreMem;
#endif
uint8_t *romPtr;

#if (defined(HW_RVL) || defined(HW_DOL))
static BasicBlock *blockTable = nullptr;
static BasicBlock **smcRegistry = nullptr;
static uint8_t* smcPageFlags = nullptr;
static mspace memspace_ptr = nullptr;
static int memoryMode = -1;
#endif

void* bootmem_calloc(size_t size)
{
#ifdef HW_RVL
	return mem2_calloc(1, size);
#else
	return calloc(1, size);
#endif
}

// Boot-time buffer allocations owned by the VBA core
void flashAllocate();
void gbColorFilterAllocate();
void gbCheatsAllocate();
void gbSgbAllocate();

void InitMemManager ()
{
#ifdef HW_RVL
	AssetsRelocate(); // first: assets are not usable before this

	romPtr = (uint8_t *)mem2_malloc(MAX_GBA_ROM_SIZE); // allocate 32 MB to GBA ROM
	texturemem = (uint8_t *)mem2_memalign(32, TEXTUREMEM_SIZE);
	blockTable = (BasicBlock*)mem2_malloc(HASH_TABLE_SIZE * sizeof(BasicBlock));
	smcRegistry = (BasicBlock **)mem2_malloc(SMC_MAP_SIZE * sizeof(void*));
	smcPageFlags = (uint8_t*)mem2_malloc(SMC_MAP_SIZE);
#elif HW_DOL
	romPtr = (uint8_t *)VM_Init(MAX_GBA_ROM_SIZE, 2 * 1024 * 1024); // 2MB MEM1 + 16 ARAM + SD backing for GB/GBA ROM
	VMPager_Init(romPtr);
#else
	romPtr = (uint8_t *)memalign(FILE_BUFFER_ALIGN, MAX_GBA_ROM_SIZE * 2);
	savebuffer = (uint8_t *)memalign(FILE_BUFFER_ALIGN, SAVEBUFFERSIZE);
	GuiImageData::setDecodeScratch(memalign(FILE_BUFFER_ALIGN, IMAGE_DECODE_SCRATCH_SIZE), IMAGE_DECODE_SCRATCH_SIZE);
	browserList = (BROWSERENTRY *)memalign(FILE_BUFFER_ALIGN, sizeof(BROWSERENTRY) * MAX_BROWSER_SIZE);
	texturemem = coreMem.gba.texturemem;
#endif

	flashAllocate();
	gbColorFilterAllocate();
	gbCheatsAllocate();
	gbSgbAllocate();
}

#ifdef __WIIU__
static bool jitProbed = false;

void InitJitWiiU() {
#if VBA_JIT
	uint32_t *arena = WutCodegenAcquire(JIT_ARENA_SIZE);
	if (arena) {
	    jitCache.initialize(arena,
	        (BasicBlock*)coreMem.gba.blockTable,
	        (BasicBlock**)coreMem.gba.smcRegistry,
	        (uint8_t*)coreMem.gba.smcPageFlags);
	}
#endif
	jitProbed = true;
}
#endif

bool JitIsAvailable()
{
#if !VBA_JIT
	return false;
#elif defined(__WIIU__)
	return jitCache.isReady();
#else
	return true;
#endif
}

void EnforceJitSetting()
{
	if(!EmuSettings.dynamicRecompilation)
		return;
#ifdef __WIIU__
	if(!jitProbed)
		return;
#endif
	if(!JitIsAvailable())
		EmuSettings.dynamicRecompilation = false;
}

void EnforceJitSettingForGame()
{
	EnforceJitSetting();

	if(EmuSettings.dynamicRecompilation && IsGBAGame() && !jitCache.isReady())
		EmuSettings.dynamicRecompilation = false;
}

#if (!defined(HW_RVL) && !defined(HW_DOL))
void* memspace_malloc(uint32_t size) { return memalign(FILE_BUFFER_ALIGN, size); }
char* memspace_strdup(const char *s) { return strdup(s); }
void memspace_free(void *ptr) { free(ptr); }
int memspace_size_free() { return 0; }
void* extmem_malloc(uint32_t size) { return memalign(FILE_BUFFER_ALIGN, size); }
void extmem_free(void *ptr) { free(ptr); }
void SwitchMemoryModeMenu() { }
void SwitchMemoryModeGame() {
#ifdef __WIIU__
	jitCache.flushCache();
#endif
}
#else
void* memspace_malloc(uint32_t size)
{
	if(!memspace_ptr) return nullptr;
	return mspace_malloc(memspace_ptr, size);
}

char* memspace_strdup(const char *s)
{
	if(!memspace_ptr) return nullptr;
	return mspace_strdup(memspace_ptr, s);
}

void memspace_free(void *ptr)
{
	if(!memspace_ptr) return;
	mspace_free(memspace_ptr, ptr);
}

int memspace_size_free()
{
	if(!memspace_ptr) return 0;
	struct mallinfo info = mspace_mallinfo(memspace_ptr);
	return info.fordblks;
}

#ifdef HW_RVL
void* extmem_malloc(uint32_t size)
{
	return mem2_malloc(size);
}

void extmem_free(void *ptr)
{
	mem2_free(ptr);
}
#else
void* extmem_malloc(uint32_t size) { return malloc(size); }
void extmem_free(void *ptr) { free(ptr); }
#endif

static bool ChangeMode(int mode) {
	if(memoryMode == mode)
		return false;

	MutexLock scratchGuard(GuiImageData::scratchLock());

	GuiImageData::setDecodeScratch(nullptr, 0);

	browserList = nullptr;
	savebuffer = nullptr;
	if(memspace_ptr) destroy_mspace(memspace_ptr);
	memspace_ptr = nullptr;
#ifdef HW_DOL
	texturemem = nullptr;
#endif
	jitCache.destroy();
	memoryMode = mode;
	return true;
}

static void CreateMem1Space(uint8_t *heapSpace, uint32_t size) {
	memspace_ptr = create_mspace_with_base(heapSpace, size, 0);
	mspace_set_footprint_limit(memspace_ptr, size);
	savebuffer = (uint8_t *)memspace_malloc(SAVEBUFFERSIZE);
}

void SwitchMemoryModeMenu() {
	if(!ChangeMode(MEMORY_MODE_MENU)) return;
	browserList = coreMem.menu.browserList;
	CreateMem1Space(coreMem.menu.heapSpace, sizeof(coreMem.menu.heapSpace));

	MutexLock scratchGuard(GuiImageData::scratchLock());
	void * decodeScratch = memspace_malloc(IMAGE_DECODE_SCRATCH_SIZE);
	GuiImageData::setDecodeScratch(decodeScratch, IMAGE_DECODE_SCRATCH_SIZE);
}

static void SwitchMemoryModeGB() {
	if(!ChangeMode(MEMORY_MODE_GB)) return;
#ifdef HW_DOL
	texturemem = coreMem.gb.texturemem;
#endif
	CreateMem1Space(coreMem.gb.heapSpace, sizeof(coreMem.gb.heapSpace));
}

static void SwitchMemoryModeGBA() {
	if(!ChangeMode(MEMORY_MODE_GBA)) return;
#ifdef HW_DOL
	texturemem = coreMem.gba.texturemem;
	blockTable = (BasicBlock*)coreMem.gba.blockTable;
	smcRegistry = (BasicBlock**)coreMem.gba.smcRegistry;
	smcPageFlags = (uint8_t*)coreMem.gba.smcPageFlags;
#endif
	jitCache.initialize(
		(uint32_t*)coreMem.gba.jitArena,
		blockTable,
		smcRegistry,
		smcPageFlags
	);
}

void SwitchMemoryModeGame() {
	if(IsGBAGame()) {
		SwitchMemoryModeGBA();
	}
	else {
		SwitchMemoryModeGB();
	}
#ifdef HW_DOL
	memset(texturemem, 0, TEXTUREMEM_SIZE);
#endif
}
#endif

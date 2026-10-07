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
#include <ogc/system.h>
#endif
#include <malloc.h>
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

/****************************************************************************
 * Wii memory layout
 *
 * MEM2 (64MB class, via libogc2's mem2_* allocator) is split into two blocks
 * grabbed once at startup:
 *   block A: exactly 32MB (JIT_ARENA_MAX_SIZE)
 *   block B: everything else that is available, minus MEM2_RESERVE_SIZE
 * Each GBA game puts its ROM in one block and its JIT arena in the other.
 * The arena is capped at 32MB because the JIT patches relative `b`
 * instructions between blocks (+/-32MB reach), so block A is the ideal arena.
 * Normally the ROM goes in B and the arena in A. If the ROM does not fit in B
 * (e.g. a 32MB ROM), the ROM goes in A and the arena gets B instead.
 *
 * MEM1 is split three ways:
 *   - coreMem: a per-mode overlay (menu / GB game / GBA game). Everything in
 *     it is thrown away and re-created on every mode switch, so nothing can
 *     leak or fragment across modes. The GBA JIT arena no longer lives here.
 *   - extmem: a small persistent heap for things that must survive mode
 *     switches (user font, background music). These are held by raw pointer
 *     (FreeType face, GuiSound) across menu <-> game transitions, so they can
 *     NOT live in the overlay - it is reused as texture/JIT-table memory.
 ***************************************************************************/
#ifdef HW_RVL
#define MEM2_BLOCK_A_SIZE		JIT_ARENA_MAX_SIZE			// 32MB, always
#define MEM2_RESERVE_SIZE		(1024*1024)					// left unclaimed for anything else that wants MEM2
#define MEM2_PROBE_STEP			(256*1024)					// granularity when probing for the largest block
#define JIT_ARENA_MIN_SIZE		(1024*1024)					// below this the JIT is not worth enabling

#define EXT_HEAP_SIZE			(4*1024*1024)				// persistent MEM1 heap (font, bg music), upper bound
#define EXT_HEAP_MIN_SIZE		(256*1024)
#define MEM1_MALLOC_HEADROOM	(6*1024*1024)				// MEM1 malloc space that must stay free after extmem is carved out

// The overlay keeps its previous footprint so the menu and GB modes see
// exactly the heap they always had; only the GBA arena (8MB) moved to MEM2.
#define MEM1_OVERLAY_SIZE		(TEXTUREMEM_SIZE + (8*1024*1024) + (HASH_TABLE_SIZE * sizeof(BasicBlock)) + SMC_MAP_SIZE + (SMC_MAP_SIZE * sizeof(void*)))
#define GB_HEAP_SIZE			(MEM1_OVERLAY_SIZE - TEXTUREMEM_SIZE)
#define MENU_HEAP_SIZE			(MEM1_OVERLAY_SIZE - (sizeof(BROWSERENTRY) * MAX_BROWSER_SIZE))
#endif

enum {
	MEMORY_MODE_NONE = -1,
	MEMORY_MODE_MENU = 0,
	MEMORY_MODE_GB,
	MEMORY_MODE_GBA
};

// Mode 3: GBA Game
struct GBAMemory {
    uint8_t texturemem[TEXTUREMEM_SIZE];
#ifndef HW_RVL
    uint32_t jitArena[JIT_ARENA_SIZE / sizeof(uint32_t)]; // Wii: arena is in MEM2
#endif
    uint8_t blockTable[HASH_TABLE_SIZE * sizeof(BasicBlock)];
    uint8_t smcPageFlags[SMC_MAP_SIZE];
    uint8_t smcRegistry[SMC_MAP_SIZE * sizeof(void*)];
} __attribute__((aligned(32)));

// Mode 2: GB Game
struct GBMemory {
	uint8_t texturemem[TEXTUREMEM_SIZE];
#ifdef HW_RVL
	uint8_t heapSpace[GB_HEAP_SIZE];
#else
	uint8_t heapSpace[sizeof(struct GBAMemory) - TEXTUREMEM_SIZE];
#endif
} __attribute__((aligned(32)));

// Mode 1: Menu
struct MenuMemory {
    BROWSERENTRY browserList[MAX_BROWSER_SIZE];
#ifdef HW_RVL
    uint8_t heapSpace[MENU_HEAP_SIZE];
#else
    uint8_t heapSpace[sizeof(struct GBAMemory) - (sizeof(BROWSERENTRY) * MAX_BROWSER_SIZE)];
#endif
} __attribute__((aligned(32)));

// The Master Overlay
union CoreMemoryOverlay {
    struct MenuMemory menu;
    struct GBMemory gb;
    struct GBAMemory gba;
};

alignas(32) union CoreMemoryOverlay coreMem;
uint8_t *romPtr;

#if (defined(HW_RVL) || defined(HW_DOL))
static mspace memspace_ptr = nullptr;
static mspace extmem_space = nullptr;
static int memoryMode = -1;
// Where the JIT arena for the currently selected ROM lives (see ROMMemoryAcquire)
static uint32_t *jitArenaBase = nullptr;
static size_t jitArenaBytes = 0;
#endif

#ifdef HW_RVL
static uint8_t *mem2BlockA = nullptr;
static uint8_t *mem2BlockB = nullptr;
static size_t mem2SizeA = 0;
static size_t mem2SizeB = 0;

// Upper bound on what mem2_malloc could still hand out in one piece right now:
// MEM2 the allocator has not claimed yet, plus free space it already owns,
// clamped by the allocator's footprint limit if one is set.
static size_t Mem2Available()
{
	struct mallinfo mi = mem2_mallinfo();
	size_t freeInHeap = (size_t)mi.fordblks;
	size_t avail = (size_t)SYS_GetArena2Size() + freeInHeap;

	size_t limit = mem2_malloc_footprint_limit();
	if(limit != 0 && limit != (size_t)-1) { // 0 / SIZE_MAX both mean "no limit"
		size_t footprint = mem2_malloc_footprint();
		size_t room = (limit > footprint) ? (limit - footprint) + freeInHeap : freeInHeap;
		if(room < avail) avail = room;
	}
	return avail;
}

// The estimate above ignores allocator overhead and rounding, so verify by
// actually allocating, backing off one step at a time.
static uint8_t* Mem2AllocLargest(size_t maxBytes, size_t *outSize)
{
	size_t size = maxBytes & ~(size_t)(MEM2_PROBE_STEP - 1);

	for(int tries = 0; tries < 64 && size >= MEM2_PROBE_STEP; tries++, size -= MEM2_PROBE_STEP) {
		void *p = mem2_memalign(32, size);
		if(p) {
			*outSize = size;
			return (uint8_t *)p;
		}
	}
	*outSize = 0;
	return nullptr;
}

static void InitMem2()
{
	mem2BlockA = (uint8_t *)mem2_memalign(32, MEM2_BLOCK_A_SIZE);
	mem2SizeA = MEM2_BLOCK_A_SIZE;

	size_t avail = Mem2Available();
	if(avail > MEM2_RESERVE_SIZE)
		mem2BlockB = Mem2AllocLargest(avail - MEM2_RESERVE_SIZE, &mem2SizeB);

	romPtr = mem2BlockB; // Sensible default until a ROM picks its layout
}

// Carve a persistent heap out of MEM1 for font / bg music / debug log.
// Sized from what is actually free so a tight MEM1 shrinks it rather than
// starving the rest of the app.
static void InitExtMem1()
{
	size_t want = EXT_HEAP_SIZE;
	struct mallinfo mi = mem1_mallinfo();
	size_t freeBytes = (size_t)mi.fordblks;
	size_t usable = (freeBytes > MEM1_MALLOC_HEADROOM) ? (freeBytes - MEM1_MALLOC_HEADROOM) : 0;
	if(want > usable) want = usable;
	want &= ~(size_t)31;

	void *base = nullptr;
	while(want >= EXT_HEAP_MIN_SIZE) {
		base = mem1_memalign(32, want);
		if(base) break;
		want /= 2;
	}
	if(!base) return;

	extmem_space = create_mspace_with_base(base, want, 1);
	mspace_set_footprint_limit(extmem_space, want);
}
#endif

void InitMemManager ()
{
#ifdef HW_RVL
	InitExtMem1();
	InitMem2(); // sets romPtr to a default; ROMMemoryAcquire() picks the real layout per ROM
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
}

#ifdef __WIIU__
static bool jitProbed = false;

void InitJitWiiU() {
#if VBA_JIT
	uint32_t *arena = WutCodegenAcquire(JIT_ARENA_SIZE);
	if (arena) {
	    jitCache.initialize(arena,
	    	JIT_ARENA_SIZE,
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
int extmem_size_free() { return 0; }
uint8_t* ROMMemoryAcquire(uint32_t romSize, uint32_t *capacity)
{
	(void)romSize;
	if(capacity) *capacity = MAX_GBA_ROM_SIZE;
	return romPtr;
}
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

void* extmem_malloc(uint32_t size)
{
	if(!extmem_space) return nullptr;
	return mspace_malloc(extmem_space, size);
}

void extmem_free(void *ptr)
{
	if(!extmem_space) return;
	mspace_free(extmem_space, ptr);
}

int extmem_size_free()
{
	if(!extmem_space) return 0;
	struct mallinfo info = mspace_mallinfo(extmem_space);
	return info.fordblks;
}

#ifdef HW_RVL
uint8_t* ROMMemoryAcquire(uint32_t romSize, uint32_t *capacity)
{
	// Unknown (0) or oversized: assume the worst so the ROM lands in a block it surely fits
	if(romSize == 0 || romSize > MAX_GBA_ROM_SIZE)
		romSize = MAX_GBA_ROM_SIZE;

	uint8_t *romBlock, *jitBlock;
	size_t romCap, jitCap;

	if(mem2BlockB && romSize <= mem2SizeB) { // usual case: ROM in the remainder, 32MB block left for the cache
		romBlock = mem2BlockB; romCap = mem2SizeB;
		jitBlock = mem2BlockA; jitCap = mem2SizeA;
	}
	else if(mem2BlockA && romSize <= mem2SizeA) { // ROM too big for B (e.g. 32MB): swap, cache gets the remainder
		romBlock = mem2BlockA; romCap = mem2SizeA;
		jitBlock = mem2BlockB; jitCap = mem2SizeB;
	}
	else {
		if(capacity) *capacity = 0;
		return nullptr;
	}

	if(jitCap > JIT_ARENA_MAX_SIZE) jitCap = JIT_ARENA_MAX_SIZE;
	jitCap &= ~(size_t)31;
	if(!jitBlock || jitCap < JIT_ARENA_MIN_SIZE) {
		jitBlock = nullptr;
		jitCap = 0;
	}
	jitArenaBase = (uint32_t *)jitBlock;
	jitArenaBytes = jitCap;

	if(romCap > MAX_GBA_ROM_SIZE) romCap = MAX_GBA_ROM_SIZE;
	romPtr = romBlock;
	if(capacity) *capacity = (uint32_t)romCap;
	return romPtr;
}
#elif defined(HW_DOL)
uint8_t* ROMMemoryAcquire(uint32_t romSize, uint32_t *capacity)
{
	(void)romSize;
	if(capacity) *capacity = MAX_GBA_ROM_SIZE;
	return romPtr;
}
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
	texturemem = nullptr;
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
	texturemem = coreMem.gb.texturemem;
	CreateMem1Space(coreMem.gb.heapSpace, sizeof(coreMem.gb.heapSpace));
}

static void SwitchMemoryModeGBA() {
	if(!ChangeMode(MEMORY_MODE_GBA)) return;
	texturemem = coreMem.gba.texturemem;
#ifdef HW_DOL
	jitArenaBase = (uint32_t*)coreMem.gba.jitArena;
	jitArenaBytes = JIT_ARENA_SIZE;
#endif

	jitCache.initialize(
		jitArenaBase,
		jitArenaBytes,
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
#endif

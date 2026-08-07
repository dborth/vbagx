/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 *
 * vmpager.c
 *
 * GameBoy Advance Hardware Virtual Memory Paging (ARAM MMU) for GameCube
 *
 * -------------------------------------------------------------------------
 * OVERVIEW
 * -------------------------------------------------------------------------
 * This file is the "Tier 0" backing-store service for the software MMU
 * implemented in vm.c: it owns the SD card file handle for the currently
 * loaded GBA ROM and is responsible for actually getting ROM bytes off the
 * SD card and into the emulated virtual address space, on demand, without
 * ever blocking the emulation/game thread on slow filesystem I/O directly.
 *
 * It exists as a companion to vm.c's DSI (page-fault) handler because
 * disk reads are far too slow to perform synchronously on the thread that
 * just took a hardware fault while emulating the GBA CPU - `fseeko`/
 * `fread` against an SD card can stall for many milliseconds, which would
 * be catastrophic for real-time GBA emulation timing. Instead:
 *
 *   1. vm.c's `vm_dsi_handler` detects a fault on a virtual page whose
 *      data has never been loaded (`virt_map[v_index].committed == 0`).
 *   2. It calls `VMPager_RequestPage(v_index)`, which just posts the page
 *      index onto a message queue (non-blocking from the pager's
 *      perspective) and returns almost immediately.
 *   3. The game thread then spins (yielding the CPU each iteration via
 *      `LWP_YieldThread`) until `virt_map[v_index].committed` becomes 1.
 *   4. Meanwhile, this file's dedicated background thread
 *      (`VMPager_ThreadFunc`, running at a lower priority, 80) wakes up,
 *      reads the requested page (plus a read-ahead block of neighbours)
 *      from the ROM file on the SD card, and `memcpy()`s it into the
 *      live virtual address range, which itself pages in real MEM1 frames
 *      via vm.c's DSI handler as it writes (see the "REENTRANT FAULT"
 *      note below) - finally calling `VM_SetCommitted()` per page to
 *      release the waiting game thread.
 *
 * This turns what would otherwise be a synchronous, timing-breaking SD
 * card read directly on the emulation thread into an asynchronous
 * producer/consumer hand-off, at the cost of the requesting thread
 * blocking (via yield-spin, not a hard wait) until the data actually
 * arrives - still far better than stalling inside the DSI trap handler
 * itself with interrupts masked.
 *
 * REENTRANT FAULT ON THE PAGER THREAD'S OWN MEMCPY
 * ----------------------------------------------------
 * `vmRomPtr` (set once via `VMPager_Init`) points at the *virtual*,
 * VM_Base-mapped base of the ROM region - the exact address range managed
 * by vm.c's software MMU. This means `memcpy(vmRomPtr + offset, ...)`
 * below is itself writing into pageable, possibly-unmapped memory: it
 * will trigger the very same DSI fault handler in vm.c, but on this
 * pager thread instead of the game thread. vm.c's handler specifically
 * checks `LWP_GetSelf() == VMPager_GetThread()` and, when true, skips the
 * "post a request and wait" branch entirely (which would deadlock the
 * pager against itself) and just faults in a physical MEM1 frame
 * immediately so the memcpy can complete. `VMPager_GetThread()` below is
 * what makes that identity check possible from vm.c.
 *
 * READ-AHEAD / BATCHING
 * -------------------------
 * Because GBA code and data access inside ROM tends to be highly local
 * (sequential instruction fetch, nearby lookup/graphics/audio tables),
 * `VMPager_ThreadFunc` does not service a single requested page in
 * isolation. Each request is expanded to the enclosing aligned
 * `PREFETCH_PAGES` (16 pages = 64KB) block, and the whole block is read
 * from the SD file and committed in one pass. This amortizes the fixed
 * cost of an `fseeko`/`fread` round trip over many pages and typically
 * pre-satisfies several *future* faults for free (they'll see
 * `committed == 1` already and never even need to queue a request).
 *
 * INITIAL BOOT PRELOAD vs LAZY ON-DEMAND PAGING
 * ---------------------------------------------------
 * `VMPager_LoadROM` performs one additional, separate thing at ROM-load
 * time: rather than relying purely on lazy fault-driven paging from a
 * cold start (which would mean the very first frames of emulation take a
 * DSI fault + SD read for nearly every ROM page touched), it
 * synchronously bulk-reads up to `ARAM_SIZE` (16MB) of the ROM straight
 * into the VM region up front, on the calling thread, before the pager
 * thread's lazy path is ever exercised, and marks all of those pages
 * committed immediately. Only ROMs (or the remainder of larger ROMs)
 * beyond that initial 16MB window rely purely on the lazy
 * fault-request-pager-commit path described above during actual gameplay.
 *
 * THREADING / SYNCHRONIZATION SUMMARY
 * ----------------------------------------
 * - `pager_queue` (an `mqbox_t` message queue) is the sole hand-off point
 *   between requesting threads (the game thread, via
 *   `VMPager_RequestPage`) and the pager thread. Sending a `u32` page
 *   index (or the sentinel `-1` for shutdown) is the entire request
 *   protocol - no request payload beyond the page index is needed since
 *   the pager re-derives everything else (aligned block bounds, byte
 *   offset/size) itself.
 * - `virt_map[].committed` (owned by vm.c, mutated via `VM_SetCommitted`)
 *   is the sole completion signal; there is deliberately no per-request
 *   response message, since multiple in-flight requests for pages inside
 *   the same read-ahead block can all be satisfied by one pager pass, and
 *   any number of stalled game-thread faults can be polling the same bit
 *   simultaneously.
 * - `VMPager_Shutdown` cleanly drains the thread by sending the `-1`
 *   sentinel and joining, rather than cancelling it, so an in-progress
 *   SD read/DMA is never torn down mid-flight.
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

// The currently open GBA ROM file on the SD card - the Tier-0
// authoritative backing store for every page vm.c ever pages in.
static FILE* romFile = NULL;
static int romSize = 0;
// Virtual-address base (inside vm.c's VM_Base-mapped region) that this
// ROM's byte offset 0 corresponds to. Writes through this pointer are
// what actually populate GBA-visible ROM memory, and are themselves
// subject to vm.c's software paging (see REENTRANT FAULT note above).
static u8* vmRomPtr;

static lwp_t pager_thread = LWP_THREAD_NULL;
static mqbox_t pager_queue = MQ_BOX_NULL;
static bool pager_running = false;

// Read-ahead granularity: each page-in request is rounded out to a
// PREFETCH_PAGES-aligned block and serviced as one batch (see the
// READ-AHEAD note in the file header).
#define PREFETCH_PAGES 16
#define PAGE_SIZE 4096
#define PAGE_BUFFER_SIZE (PREFETCH_PAGES * PAGE_SIZE)
#define PAGER_STACK_SIZE (64 * 1024)

static u8* pager_stack = NULL;
// Staging buffer the pager thread fread()s a batch into before copying
// it (page by page, via the reentrant-faulting memcpy) into the live VM
// region - keeps the SD read itself as one contiguous I/O op regardless
// of how the destination pages end up physically backed.
static u8* page_buffer = NULL;

// Identity accessor used by vm.c's DSI handler to detect "is the thread
// that just faulted the pager thread itself?" - see the REENTRANT FAULT
// note above for why that distinction is safety-critical.
lwp_t VMPager_GetThread(void) {
	return pager_thread;
}

// Background worker thread body. Blocks on the message queue between
// requests; for each page index received, expands it to its containing
// PREFETCH_PAGES-aligned block (clamped to the ROM's actual page count),
// reads that whole block from the SD file into `page_buffer`, then
// memcpy()s it into the live virtual ROM range at `vmRomPtr + offset`
// (which pages in real MEM1 frames as it writes - see the REENTRANT
// FAULT note above), and finally calls VM_SetCommitted() once per page
// actually read so every game-thread fault waiting on any page in this
// block can proceed.
static void* VMPager_ThreadFunc(void* arg) {
	while (pager_running) {
		mqmsg_t msg;

		if (MQ_Receive(pager_queue, &msg, MQ_MSG_BLOCK) == FALSE) continue;

		if ((s32)(u32)msg == -1 || !pager_running) break; // Shutdown signal

		u16 req_v_index = (u16)(u32)msg;
		if (VM_IsCommitted(req_v_index)) continue;

		// Align the fetch index to our prefetch block size to avoid overlapping reads
		u16 start_v_index = (req_v_index / PREFETCH_PAGES) * PREFETCH_PAGES;
		u16 max_pages = (romSize + PAGE_SIZE - 1) / PAGE_SIZE;

		u16 end_v_index = start_v_index + PREFETCH_PAGES;
		if (end_v_index > max_pages) end_v_index = max_pages;

		u32 offset = start_v_index * PAGE_SIZE;
		u32 readSize = (end_v_index - start_v_index) * PAGE_SIZE;
		if (offset + readSize > (u32)romSize) readSize = romSize - offset;

		if (romFile) {
			fseeko(romFile, offset, SEEK_SET);
			fread(page_buffer, 1, readSize, romFile);
			// This memcpy will intentionally trigger DSI exceptions on the pager thread
			memcpy(vmRomPtr + offset, page_buffer, readSize);
			// Explicitly mark these pages as committed now that MEM1 is populated
			u32 pages_read = (readSize + PAGE_SIZE - 1) / PAGE_SIZE;
			for (u32 i = 0; i < pages_read; i++) {
				VM_SetCommitted(start_v_index + i);
			}
		}
	}
	return NULL;
}

// Allocates the pager thread's stack and staging buffer, brings up the
// request message queue, and spawns VMPager_ThreadFunc at priority 80 (a
// background/low priority relative to emulation). `ptr` must be the
// VM_Base-relative virtual address that ROM offset 0 maps to; it is
// stashed as `vmRomPtr` for every subsequent read to write through.
// Idempotent: a no-op if the pager is already running.
void VMPager_Init(u8 *ptr) {
	if (pager_running) return;

	pager_stack = (u8*)memalign(32, PAGER_STACK_SIZE);
	page_buffer = (u8*)memalign(32, PAGE_BUFFER_SIZE);

	MQ_Init(&pager_queue, 128);

	pager_running = true;
	LWP_CreateThread(&pager_thread, VMPager_ThreadFunc, NULL, pager_stack, PAGER_STACK_SIZE, 80);

	vmRomPtr = ptr;
}

// Cleanly stops the pager thread: clears the running flag, posts the `-1`
// shutdown sentinel (waking the thread out of its blocking MQ_Receive
// even if no real request is pending), joins it to guarantee any
// in-flight SD read/DMA has fully completed before anything is freed,
// then tears down the queue and both buffers.
void VMPager_Shutdown() {
	if (!pager_running) return;
	pager_running = false;

	// Wake and terminate thread
	MQ_Send(pager_queue, (mqmsg_t)-1, MQ_MSG_BLOCK);
	LWP_JoinThread(pager_thread, NULL);

	MQ_Close(pager_queue);
	free(pager_stack);
	free(page_buffer);

	pager_queue = MQ_BOX_NULL;
	pager_stack = NULL;
	page_buffer = NULL;
	pager_thread = LWP_THREAD_NULL;
}

// Non-blocking (from the caller's point of view - MQ_MSG_BLOCK only
// blocks if the queue itself is momentarily full) request for a page's
// data to be loaded. Called by vm.c's DSI handler when a page fault hits
// a never-committed page; the caller is expected to poll
// VM_IsCommitted()/virt_map[].committed for completion afterward, not to
// wait on any reply from this call.
void VMPager_RequestPage(u16 v_index) {
	if (pager_running && pager_queue != MQ_BOX_NULL) {
		MQ_Send(pager_queue, (mqmsg_t)(u32)v_index, MQ_MSG_BLOCK);
	}
}

// Opens a ROM file from the SD card and performs the initial bulk
// preload described in the "INITIAL BOOT PRELOAD" section of the file
// header: closes any currently open ROM, determines the file's size,
// resets the whole VM/ARAM/PTE state via VM_Clear(), then synchronously
// reads up to ARAM_SIZE (16MB) worth of ROM data into the live VM region
// in PAGE_BUFFER_SIZE-sized chunks (reusing the same staging buffer and
// reentrant-faulting memcpy path the background pager thread uses),
// marking every page it touches committed immediately. Any remainder of
// a ROM larger than that initial window is left to be paged in lazily,
// on demand, by the background thread during actual gameplay. Returns
// the ROM size in bytes on success, or 0 on failure to open/size the file.
int VMPager_LoadROM(const char * filepath) {
	VMPager_CloseFile();
	
	romFile = fopen(filepath, "rb");
	if (romFile == NULL) {
		return 0;
	}

	fseeko(romFile, 0, SEEK_END);
	romSize = ftello(romFile);
	fseeko(romFile, 0, SEEK_SET);

	if(romSize <= 0) {
		fclose(romFile);
		romFile = NULL;
		romSize = 0;
		return 0;
	}

	VM_Clear();

	u32 offset = 0;
	// Only preload up to 16MB (ARAM limit) at boot
	u32 preloadSize = (romSize > ARAM_SIZE) ? (ARAM_SIZE) : romSize;

	while (offset < preloadSize) {
		u32 readSize = preloadSize - offset;
		if (readSize > PAGE_BUFFER_SIZE) readSize = PAGE_BUFFER_SIZE;

		fread(page_buffer, 1, readSize, romFile);
		memcpy(vmRomPtr + offset, page_buffer, readSize);
		offset += readSize;
	}

	u32 pages_read = (preloadSize + PAGE_SIZE - 1) / PAGE_SIZE;
	for (u32 i = 0; i < pages_read; i++) {
		VM_SetCommitted(i);
	}

	return romSize;
}

// Closes the currently open ROM file, if any, and resets the cached size.
// Does not touch VM/ARAM/PTE state - callers that need a full reset
// should go through VMPager_LoadROM (which calls VM_Clear()) or VM_Clear
// directly.
void VMPager_CloseFile() {
	if (romFile) {
		fclose(romFile);
		romFile = NULL;
		romSize = 0;
	}
}
#endif

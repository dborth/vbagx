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
 *   2. It calls `VMPager_RequestAndWaitPage(v_index)`, which posts the
 *      page index onto a message queue and blocks the calling thread on
 *      a condition variable until that page is committed.
 *   3. Meanwhile, this file's dedicated background thread
 *      (`VMPager_ThreadFunc`) wakes up, reads the requested page (plus a
 *      read-ahead block of neighbours) from the ROM file on the SD card,
 *      and `memcpy()`s it into the live virtual address range, which
 *      itself pages in real MEM1 frames via vm.c's DSI handler as it
 *      writes (see the "REENTRANT FAULT" note below) - finally calling
 *      `VM_SetCommitted()` per page to release any waiter.
 *
 * This turns what would otherwise be a synchronous, timing-breaking SD
 * card read directly on the emulation thread into an asynchronous
 * producer/consumer hand-off: the requesting thread blocks on a condvar
 * (not a busy-spin) until the data actually arrives, and other threads
 * (including lower-priority ones, like this pager) are free to run while
 * it waits.
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
 * A request can legitimately land on a page beyond the ROM's real size:
 * GBA carts are freely mirrored/open-bus past their actual end, and
 * vm_dsi_handler has no notion of the loaded ROM's size - it only knows
 * the page is somewhere inside the full 256MB VM window. Such requests
 * are recognized and short-circuited (see `VMPager_ThreadFunc` below):
 * there is nothing to fetch from disk for it, so the page is simply
 * published as committed against whatever zero-filled MEM1 frame vm.c
 * already mapped for it.
 *
 * INITIAL BOOT PRELOAD vs LAZY ON-DEMAND PAGING
 * ---------------------------------------------------
 * Rather than relying purely on lazy fault-driven paging from a cold
 * start (which would mean the very first frames of emulation take a DSI
 * fault + SD read for nearly every ROM page touched), the ROM-loading
 * caller bulk-reads up to `ARAM_SIZE` (16MB) of the ROM into the VM
 * region itself, up front, before the pager thread's lazy path is ever
 * exercised. This file exposes the three calls that make that safe:
 *
 *   1. `VMPager_StartPreload(file, size)` hands this pager the ROM's
 *      already-open file handle and size, resets all VM/ARAM/PTE state
 *      via `VM_Clear()`, and sets `is_preloading` so `vm.c`'s DSI
 *      handler maps blank frames instantly for the caller's own writes
 *      instead of routing them through the pager thread.
 *   2. The caller streams ROM data into the VM region itself, calling
 *      `VMPager_CommitPageRange(start_page, end_page)` after each chunk
 *      to publish those pages as committed.
 *   3. If the whole ROM fit in that initial window, the file is closed
 *      via VMPager_CloseFile - there's nothing left to stream.
 *      Otherwise if the ROM is larger than `ARAM_SIZE`, we call
 *      `VMPager_CompletePreload()` to clear `is_preloading`. The file
 *      remains open and the remainder is paged in lazily by the
 *      background thread via the fault-request-pager-commit path
 *      described above during actual gameplay.
 *
 * THREADING / SYNCHRONIZATION SUMMARY
 * ----------------------------------------
 * - `pager_queue` (an `mqbox_t` message queue) is the sole hand-off point
 *   between requesting threads (the game thread, via
 *   `VMPager_RequestAndWaitPage`) and the pager
 *   thread. Sending a `u32` page index (or the sentinel `-1` for
 *   shutdown) is the entire request protocol - no request payload
 *   beyond the page index is needed since the pager re-derives
 *   everything else (aligned block bounds, byte offset/size) itself.
 * - `virt_map[].committed` (owned by vm.c, mutated via `VM_SetCommitted`)
 *   is the completion signal that waiters actually check; `pager_cond`/
 *   `pager_mutex` are only the mechanism used to sleep/wake efficiently
 *   instead of busy-polling that flag. There is deliberately no
 *   per-request response message, since multiple in-flight requests for
 *   pages inside the same read-ahead block can all be satisfied by one
 *   pager pass, and any number of blocked waiters can be woken by one
 *   broadcast.
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
#include <ogc/cond.h>
#include <ogc/mutex.h>

#include "vmpager.h"
#include "vm.h"

// The currently open GBA ROM file on the SD card - the Tier-0
// authoritative backing store for every page vm.c ever pages in. This
// will never be a GB file because those can be read fully into ARAM.
static FILE* romFile = NULL;
static int fileSize = 0;
static char romFilepath[1024] = { 0 };
// Virtual-address base (inside vm.c's VM_Base-mapped region) that this
// ROM's byte offset 0 corresponds to. Writes through this pointer are
// what actually populate GBA-visible ROM memory, and are themselves
// subject to vm.c's software paging (see REENTRANT FAULT note above).
static u8* vmRomPtr = NULL;

static lwp_t pager_thread = LWP_THREAD_NULL;
static mqbox_t pager_queue = MQ_BOX_NULL;
static cond_t pager_cond = LWP_COND_NULL;
static mutex_t pager_mutex = LWP_MUTEX_NULL;

static bool pager_running = false;
static bool is_preloading = false;

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
// of how the destination pages end up physically backed. Every size fed
// into fread()/memcpy() against this buffer is clamped to
// PAGE_BUFFER_SIZE (see VMPager_ThreadFunc) since this is the buffer's
// hard physical capacity.
static u8* page_buffer = NULL;

// Identity accessor used by vm.c's DSI handler to detect "is the thread
// that just faulted the pager thread itself?" - see the REENTRANT FAULT
// note above for why that distinction is safety-critical.
lwp_t VMPager_GetThread(void) {
	return pager_thread;
}

// Wakes anyone blocked in VMPager_RequestAndWaitPage. Broken out since
// every exit path in VMPager_ThreadFunc below needs to do this.
static void VMPager_Notify(void) {
	LWP_MutexLock(pager_mutex);
	LWP_CondBroadcast(pager_cond);
	LWP_MutexUnlock(pager_mutex);
}

// Background worker thread body. Blocks on the message queue between
// requests; for each page index received, expands it to its containing
// PREFETCH_PAGES-aligned block (clamped to the ROM's actual page count),
// reads that whole block from the SD file into `page_buffer`, then
// memcpy()s it into the live virtual ROM range at `vmRomPtr + offset`
// (which pages in real MEM1 frames as it writes - see the REENTRANT
// FAULT note above), and finally calls VM_SetCommitted() once per page
// actually backed by real data so every waiter on any page in this block
// can proceed.
static void* VMPager_ThreadFunc(void* arg) {
	while (pager_running) {
		mqmsg_t msg;

		if (MQ_Receive(pager_queue, &msg, MQ_MSG_BLOCK) == FALSE) continue;

		if ((s32)(u32)msg == -1 || !pager_running) break; // Shutdown signal

		u16 req_v_index = (u16)(u32)msg;
		if (VM_IsCommitted(req_v_index)) {
			VMPager_Notify();
			continue;
		}

		u16 max_pages = (u16)((fileSize + PAGE_SIZE - 1) / PAGE_SIZE);

		// A request beyond the ROM's real page count is normal GBA
		// behaviour (mirrored/open-bus reads past cart end), not an
		// error condition - vm_dsi_handler has no notion of ROM size,
		// only of the full VM window. There is nothing to fetch from
		// disk for it: vm.c has already faulted in a zero-filled MEM1
		// frame, so just publish it as committed.
		if (req_v_index >= max_pages) {
			VM_SetCommitted(req_v_index);
			VMPager_Notify();
			continue;
		}

		// Align the fetch index to our prefetch block size to avoid
		// overlapping reads. Because req_v_index < max_pages (checked
		// above) and start_v_index <= req_v_index, start_v_index is
		// always < max_pages too, so clamping end_v_index to max_pages
		// below can never push it below start_v_index.
		u16 start_v_index = (req_v_index / PREFETCH_PAGES) * PREFETCH_PAGES;
		u16 end_v_index = start_v_index + PREFETCH_PAGES;
		if (end_v_index > max_pages) end_v_index = max_pages;

		u32 offset = (u32)start_v_index * PAGE_SIZE;
		u32 readSize = (u32)(end_v_index - start_v_index) * PAGE_SIZE;
		if (offset + readSize > (u32)fileSize) readSize = (u32)fileSize - offset;

		// readSize can never legitimately exceed the staging buffer's
		// capacity; clamp explicitly right before it's handed to
		// fread(), which has no awareness of page_buffer's true size.
		if (readSize > PAGE_BUFFER_SIZE) readSize = PAGE_BUFFER_SIZE;

		u32 pages_in_block = (u32)(end_v_index - start_v_index);

		if (romFile == NULL || fileSize == 0) {
			VMPager_CloseFile(); // Ensure internal state is reset if fileSize is 0 but handle is open

			// Still commit so nothing waits on this forever
			for (u16 v = start_v_index; v < end_v_index; v++) {
				VM_SetCommitted(v);
			}
		} else if (readSize > 0) {
			size_t bytesRead = 0;

			if (fseeko(romFile, offset, SEEK_SET) != 0) {
				VMPager_CloseFile(); // Seek failed: invalidate file handle so future calls fail fast
			} else {
				bytesRead = fread(page_buffer, 1, readSize, romFile);
				if (bytesRead == 0) {
					// Read failed: invalidate file handle
					VMPager_CloseFile();
				} else {
					// This memcpy will intentionally trigger DSI exceptions
					// on the pager thread - see the REENTRANT FAULT note.
					memcpy(vmRomPtr + offset, page_buffer, bytesRead);
				}
			}

			// Only commit as many pages as were actually backed by real file data.
			u32 pages_read = (u32)(bytesRead + PAGE_SIZE - 1) / PAGE_SIZE;
			if (pages_read > pages_in_block) pages_read = pages_in_block;

			for (u32 i = 0; i < pages_read; i++) {
				VM_SetCommitted(start_v_index + i);
			}
			// A short/truncated read (SD hiccup, EOF landing mid-block, etc.) still needs the rest
			// of the block published as committed, so a waiter on any page in the tail is never left
			// blocked indefinitely. Worst case here is stale/zeroed data for those trailing pages.
			for (u32 i = pages_read; i < pages_in_block; i++) {
				VM_SetCommitted(start_v_index + i);
			}
		} else {
			// No file open, or nothing left to read for this block -
			// still commit so nothing waits on this forever.
			for (u16 v = start_v_index; v < end_v_index; v++) {
				VM_SetCommitted(v);
			}
		}

		VMPager_Notify();
	}
	return NULL;
}

// Allocates the pager thread's stack and staging buffer, brings up the
// request message queue and completion condvar, and spawns
// VMPager_ThreadFunc. `ptr` must be the VM_Base-relative virtual address
// that ROM offset 0 maps to; it is stashed as `vmRomPtr` for every
// subsequent read to write through. Idempotent: a no-op if the pager is
// already running.
void VMPager_Init(u8 *ptr) {
	if (pager_running) return;

	pager_stack = (u8*)memalign(32, PAGER_STACK_SIZE);
	page_buffer = (u8*)memalign(32, PAGE_BUFFER_SIZE);

	MQ_Init(&pager_queue, 128);
	LWP_MutexInit(&pager_mutex, false);
	LWP_CondInit(&pager_cond);

	pager_running = true;
	LWP_CreateThread(&pager_thread, VMPager_ThreadFunc, NULL, pager_stack, PAGER_STACK_SIZE, 40);

	vmRomPtr = ptr;
}

// Cleanly stops the pager thread: clears the running flag, posts the `-1`
// shutdown sentinel (waking the thread out of its blocking MQ_Receive
// even if no real request is pending), joins it to guarantee any
// in-flight SD read/DMA has fully completed before anything is freed,
// then tears down the queue, condvar/mutex, and both buffers.
void VMPager_Shutdown(void) {
	if (!pager_running) return;
	pager_running = false;

	// Wake and terminate thread
	MQ_Send(pager_queue, (mqmsg_t)-1, MQ_MSG_BLOCK);
	LWP_JoinThread(pager_thread, NULL);

	MQ_Close(pager_queue);
	LWP_CondDestroy(pager_cond);
	LWP_MutexDestroy(pager_mutex);

	free(pager_stack);
	free(page_buffer);

	pager_queue = MQ_BOX_NULL;
	pager_cond = LWP_COND_NULL;
	pager_mutex = LWP_MUTEX_NULL;
	pager_stack = NULL;
	page_buffer = NULL;
	pager_thread = LWP_THREAD_NULL;
}

// Requests a page's data and blocks the calling thread on `pager_cond`
// until VM_IsCommitted(v_index) is true (or the pager is shut down out
// from under the wait). This is the primary path vm.c's DSI handler
// uses: a real condition-variable sleep rather than a busy-spin, so the
// CPU is available to other threads - including this pager thread -
// while the calling thread waits. MQ_Send is issued before the mutex is
// taken so a full queue blocking on send can never be holding
// `pager_mutex` while it does so.
void VMPager_RequestAndWaitPage(u16 v_index) {
	if (!pager_running || pager_queue == MQ_BOX_NULL) return;

	MQ_Send(pager_queue, (mqmsg_t)(u32)v_index, MQ_MSG_BLOCK);

	LWP_MutexLock(pager_mutex);
	while (!VM_IsCommitted(v_index) && pager_running) {
		LWP_CondWait(pager_cond, pager_mutex);
	}
	LWP_MutexUnlock(pager_mutex);
}

bool VMPager_IsPreloading(void) {
	return is_preloading;
}

// Begins a caller-driven preload: closes any previously open ROM, takes
// ownership of `file` (an already-open handle positioned however the
// caller likes - this pager only ever seeks explicitly before its own
// reads, so starting position doesn't matter) and `size` (the ROM's
// total byte size), resets all VM/ARAM/PTE state via VM_Clear(), and
// sets `is_preloading` so vm.c's DSI handler maps blank frames instantly
// for the caller's own writes into the VM region instead of routing them
// through the pager thread's request/wait path. See the "INITIAL BOOT
// PRELOAD vs LAZY ON-DEMAND PAGING" section of the file header for the
// full three-call handshake this is step one of.
//
// file will be NULL if it's less than 16MB and  `size` is not validated
// (callers are expected to have already rejected an oversized file
// themselves)
void VMPager_StartPreload() {
	is_preloading = true;
	VMPager_CloseFile();
	VM_Clear();
}

// Marks virtual pages `[start_page, end_page)` - a half-open range, so
// `end_page` itself is not committed - as committed. Called by a
// preloading caller after it has written the corresponding data into
// the VM region itself (see VMPager_StartPreload); this is what makes
// those pages visible to any fault that lands on them afterward.
// Clamped to the 16-bit virtual page index space this pager (and vm.c)
// operates over. Broadcasts on `pager_cond` afterward so any thread
// already blocked in VMPager_RequestAndWaitPage for one of these pages
// - normally none, since is_preloading routes faults away from that
// path during a preload, but this makes no assumption about that -
// wakes promptly rather than waiting on an unrelated future event.
void VMPager_CommitPageRange(u32 start_page, u32 end_page) {
	if (end_page > 65536) end_page = 65536;

	for (u32 i = start_page; i < end_page; i++) {
		VM_SetCommitted((u16)i);
	}

	VMPager_Notify();
}

// Ends the preload phase started by VMPager_StartPreload(): always
// clears `is_preloading`, so any subsequent fault on an uncommitted page
// goes back through the normal VMPager_RequestAndWaitPage() path.
void VMPager_EndPreload() {
	romFile = NULL;
	fileSize = 0;
	is_preloading = false;
}

void VMPager_EndPreloadWithFile(FILE* file, u32 size, const char* path) {
	romFile = file;
	fileSize = (int)size;
	snprintf(romFilepath, 1024, "%s", path);
	is_preloading = false;
}

// Closes the currently open ROM file, if any, and resets the file size.
// Does not touch VM/ARAM/PTE state - callers that need a full reset
// should go through VMPager_StartPreload (which calls VM_Clear()) or
// VM_Clear directly.
// Also called when the file is completely loaded into ARAM
void VMPager_CloseFile(void) {
	is_preloading = false;

	if (romFile) {
		fclose(romFile);
		romFile = NULL;
		romFilepath[0] = '\0';
		fileSize = 0;
	}
}

// Pauses the VM pager by closing the file handle, but leaving fileSize intact
// so we know it needs to be reopened later.
void VMPager_Pause(void) {
	if (romFile) {
		fclose(romFile);
		romFile = NULL;
	}
}

// Resumes the VM pager by providing a newly reopened file handle.
bool VMPager_Resume() {
	if(fileSize == 0)
		return true;

	if (romFile) {
		fclose(romFile);
	}
	romFile = fopen(romFilepath, "rb");
	return romFile == NULL ? false : true;
}
#endif

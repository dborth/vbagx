/**
 * vm.c - Software MMU / Virtual Memory backend for VBA-GX (GameCube)
 *
 * Original copyright (C) 2012  tueidj
 * ISFS code replaced with ARAM code by emu_kidid
 *
 * -------------------------------------------------------------------------
 * SD-BACKED PAGING EXTENSION (2026) - Daryl Borth
 * -------------------------------------------------------------------------
 * This revision was written by Daryl Borth for the VBA-GX project to lift
 * the effective virtual-address-space ceiling from "whatever fits in ARAM"
 * up to the full MAX_VM_SIZE (256MB), by adding a third, larger, and slower
 * tier underneath ARAM: the GBA ROM file sitting on the SD card, streamed
 * in on demand by a dedicated background thread (see vmpager.c).
 *
 * WHY THIS PROJECT EXISTS AT ALL - ENABLING THE JIT
 * ------------------------------------------------------
 * This whole hardware-page-fault-driven approach (this file, plus
 * vmpager.c) was undertaken solely to make large-ROM paging compatible
 * with the VBA-GX THUMB JIT. The two memory execution paths in this emulator 
 * have fundamentally different access patterns:
 *
 *   - The C++ interpreter previously handled paging by having its own
 *     memory accessors - the CPURead/CPUWrite family of functions in VBA
 *     itself - check page residency and, on a miss, trigger a load from
 *     the SD file inline before the access completed. Every single GBA
 *     memory access already went through a C function call, so adding a
 *     residency check and a conditional load inside that call was cheap
 *     and natural.
 *   - The JIT does not work this way. Compiled THUMB blocks perform *direct*
 *     memory reads/writes: bank-check, page/mask lookup, null-pointer guard, 
 *     then a raw load/store straight through a pointer derived from the
 *     `readPages`/`readMasks` tables (R30/R31) - never a call back into a
 *     C++ accessor function per access. That directness is precisely what
 *     makes the JIT fast; routing every compiled memory access through a
 *     paging-aware C function call would eliminate the JIT's entire
 *     performance advantage over the interpreter, and would also mean
 *     teaching the JIT's code emitter to understand, block on, and resume
 *     around asynchronous SD-card I/O mid-instruction.
 *
 * Hardware paging sidesteps that problem entirely: the JIT's raw
 * loads/stores just target real (mapped-or-not) addresses in this VM
 * region exactly like any other code would, and the PowerPC MMU itself -
 * not the JIT - is what notices a page isn't resident and raises a DSI.
 * `vm_dsi_handler` then resolves that fault (fetching from the SD file
 * via vmpager.c if needed) completely transparently to the JIT, which
 * never needs to know paging exists. This is why this system was built:
 * without it, supporting GBA ROMs/working sets larger than what can fit in
 * MEM 1 would have required making the JIT itself paging-aware, which was
 * not a viable direction.
 *
 * WHY THE SD-BACKED EXTENSION SPECIFICALLY WAS NECESSARY
 * ------------------------------------------------------------
 * The original vm.c implemented a classic two-tier software MMU:
 *
 *      MEM1 (physical RAM frames, `phys_map`)  <-->  ARAM (`AR_StartDMA`)
 *
 * Pages evicted from the small pool of real MEM1 physical frames were
 * DMA'd out to ARAM at a *fixed* offset equal to `v_index * PAGE_SIZE`,
 * and fetched back the same way. That 1:1 mapping between a virtual page
 * index and an ARAM byte offset is only correct as long as the entire
 * virtual address range in active use fits inside physical ARAM (~16MB on
 * Wii). Nothing stopped `MAX_VM_SIZE` from being defined as 256MB, but any
 * v_index whose `v_index * PAGE_SIZE` offset fell outside real ARAM would
 * be completely ignored. ARAM was therefore a hidden, un-enforced cap
 * on how much of the declared VM space could actually be used.
 *
 * THE NEW THREE-TIER HIERARCHY
 * ------------------------------
 * This file now implements three tiers, each strictly smaller/faster than
 * the one below it, with the SD file as the single source of truth:
 *
 *   Tier 0 - SD card file (GBA ROM image) up to 256MB, authoritative
 *              read sequentially/on-demand by the pager thread (vmpager.c)
 *   Tier 1 - ARAM (`aram_map` / `v_to_aram`) ~16MB, write-back L2 cache
 *              for MEM1 pages the emulator has *written to* (dirty pages -
 *              e.g. GBA WRAM/SRAM regions mapped into this VM space).
 *              Slots are now indirected through `aram_map`/`v_to_aram`
 *              instead of a fixed v_index*PAGE_SIZE offset, and are
 *              recycled FIFO-style via `aram_head` when ARAM fills up -
 *              this is what decouples ARAM residency from VM size and
 *              makes >16MB virtual spaces safe.
 *   Tier 2 - MEM1 physical frames (`phys_map`, `MEM_Base`) the small,
 *              fast, PPC-hardware-mapped working set. Managed exactly as
 *              before via a software hashed page table (`HTABORG`/PTEs)
 *              and a second-chance/clock eviction scan (`locate_oldest`).
 *
 * Read path on a fault (see `vm_dsi_handler`):
 *   1. Never-loaded page (`committed == 0`)  -> ask the pager thread to
 *      read it from the SD ROM file straight into a freshly faulted-in
 *      MEM1 frame (see "COMMITTED" note below and vmpager.c).
 *   2. Previously-evicted, ARAM-resident page (`committed == 1` and
 *      `v_to_aram[v_index] != 0xFFFF`) -> DMA back from ARAM, as before.
 *   3. Anything else -> zero-fill (should not normally happen once a page
 *      has gone through the pager, but mirrors the original code's
 *      defensive fallback).
 *
 * REDEFINITION OF THE "committed" BIT
 * -------------------------------------
 * In the original code, `virt_map[v_index].committed` meant "this page has
 * been evicted to ARAM at least once, so ARAM holds valid data for it"
 * (as opposed to a never-touched page, which should be zero-filled).
 *
 * In this version, `committed` means "the authoritative content for this
 * page (the GBA ROM bytes from the SD file) has actually been loaded",
 * fully decoupled from *where* that data currently lives (MEM1 frame,
 * ARAM cache slot, or nowhere yet). This is what lets `vm_dsi_handler`
 * distinguish "never fetched from disk, ask the pager" from "was fetched
 * before and is now sitting in ARAM, just DMA it back" from "was fetched
 * before, then bumped out of ARAM by another page and never written
 * again, so a fresh disk read is safe/cheap and simplest" (the
 * `ClearMEM1Mapping` path, see below).
 *
 * GAME THREAD vs PAGER THREAD - AVOIDING SELF-DEADLOCK
 * --------------------------------------------------------
 * Fetching a page from the SD file happens by having the pager thread
 * `memcpy()` freshly-read file bytes into `vmRomPtr + offset`, which is
 * itself a pointer *into this same VM_Base-mapped virtual region*. That
 * memcpy will therefore also take a DSI fault (the destination page isn't
 * resident in MEM1 yet either) - but that fault is taken *on the pager
 * thread itself*, re-entering this very handler while the pager thread is
 * mid-request. If that inner fault tried to go through the normal
 * "request the page and block until committed" path, the pager thread
 * would be blocking on itself and the whole VM would deadlock forever.
 *
 * `vm_dsi_handler` avoids this by checking `LWP_GetSelf() ==
 * VMPager_GetThread()`: when the faulting thread *is* the pager thread,
 * it skips straight past the "wait for commit" branch and falls through
 * to the normal fault-resolution code below, which simply faults in a
 * zero-filled (or ARAM-recycled) MEM1 frame and returns immediately so
 * the pager's memcpy can proceed and write the real file data into it.
 * The pager explicitly calls `VM_SetCommitted()` per page afterwards -
 * `committed` is therefore only ever set once real ROM bytes are behind
 * that page, never merely because a physical frame exists for it.
 *
 * ARAM SLOT RECYCLING & `ClearMEM1Mapping`
 * -------------------------------------------
 * Because ARAM is now a generic, indirected cache rather than a 1:1
 * mirror of the VM address space, evicting a dirty MEM1 page may need to
 * steal an ARAM slot that currently belongs to some *other* v_index
 * (`old_occupant`) whose data is about to be overwritten. `ClearMEM1Mapping`
 * is the helper that safely severs that old_occupant's association with
 * both the ARAM slot (already unlinked by the caller via `v_to_aram`) and,
 * defensively, any lingering MEM1 mapping/PTE it might still hold, and
 * resets its `committed` flag to 0 so a later access simply re-reads clean
 * data from the SD file via the pager rather than risking stale state.
 *
 * SD-FILE READ-AHEAD
 * --------------------
 * The pager thread (vmpager.c) does not fetch single 4KB pages in
 * isolation - it aligns each request to a `PREFETCH_PAGES` (16-page /
 * 64KB) block and reads/commits the whole aligned block in one go, since
 * GBA ROM access inside a running game is highly local (sequential code
 * fetch, nearby data tables), so pre-warming neighboring pages avoids
 * repeated DSI/round-trip overhead for what would otherwise be several
 * back-to-back individual faults.
 *
 * Everything else in this file - the hashed-page-table PTE management
 * (`StorePTE`/`CalcPTEG`/`insert_pte`), the clock-style MEM1 eviction scan
 * (`locate_oldest`), and `VM_Init`/`VM_Deinit` bootstrapping - is the same
 * mechanism tueidj/emu_kidid originally wrote; only the tlbie() call was
 * hardened with an eieio/tlbsync/eieio sequence for correctness on
 * Broadway, and `VM_Init`'s one-time setup was split out into a separate,
 * re-invokable `VM_Clear()` (needed so a new ROM can be loaded and the
 * whole VM/ARAM/PTE state reset without a full re-init) plus the new
 * `VM_IsCommitted`/`VM_SetCommitted` accessors the pager thread uses to
 * query and publish page-load completion.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
**/

#ifdef HW_DOL
#include <gccore.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ogc/machine/processor.h>
#include <ogc/aram.h>
#include <ogc/context.h>
#include "vm.h"
#include "vmpager.h"

// maximum virtual memory size
#define MAX_VM_SIZE      (256*1024*1024)
// maximum physical memory size
#define MAX_MEM_SIZE     (  8*1024*1024)
// minimum physical memory size
#define MIN_MEM_SIZE     (256*1024)
// page size as defined by hardware
#define PAGE_SIZE        4096
#define PAGE_MASK        (~(PAGE_SIZE-1))

#define VM_VSID          0
#define VM_SEGMENT       0x70000000

// use 64KB for PTEs
#define HTABMASK         0
#define PTE_SIZE         ((HTABMASK+1)*65536)
#define PTE_COUNT        (PTE_SIZE>>3)

// Number of 4KB slots physical ARAM can hold - the size of the Tier-1
// write-back cache described in the header above. Independent of VM size.
#define ARAM_MAX_SLOTS   (ARAM_SIZE / PAGE_SIZE)

// keeps a record of each currently mapped page
typedef union
{
	u32 data;
	struct
	{
		u32 valid      :  1;
		u32 locked     :  1;
		u32 dirty      :  1;
		u32 pte_index  : 13;
		u32 page_index : 16;
	};
} p_map;

// maps VM addresses to mapped pages
typedef struct
{
	// data must be fetched when paging in?
	u16 committed  :  1;
	u16 p_map_index: 12;
} vm_map;

typedef union
{
	u32 data[2];
	struct
	{
		u32 valid  :  1;
		u32 VSID   : 24;
		u32 hash   :  1;
		u32 API    :  6;

		u32 RPN    : 20;
		u32 pad0   :  3;
		u32 R      :  1;
		u32 C      :  1;
		u32 WIMG   :  4;
		u32 pad1   :  1;
		u32 PP     :  2;
	};
} PTE;
typedef PTE* PTEG;

typedef u8 vm_page[PAGE_SIZE];

// --- Tier 2: MEM1 physical-frame bookkeeping ---
static p_map phys_map[2048+(PTE_SIZE/PAGE_SIZE)];
// --- Per-virtual-page state; `committed` means "loaded from the SD
// ROM file" (see header note above), independent of where that data
// currently resides (MEM1 frame, ARAM cache slot, or nowhere yet) ---
static vm_map virt_map[65536];
static u16 pmap_max, pmap_head;

static PTE* HTABORG;
static vm_page* VM_Base;
static vm_page* MEM_Base = NULL;

static mutex_t vm_mutex = LWP_MUTEX_NULL;
static u32 VMSize = 0;
static u32 MEMSize = 0;
static bool vm_initialized = 0;

// --- Tier 1: ARAM write-back cache indirection tables ---
// Tracks which v_index currently resides in each ARAM slot
static u16 aram_map[ARAM_MAX_SLOTS];
// Tracks which ARAM slot holds a given v_index
static u16 v_to_aram[65536];
// Simple FIFO clock hand for eviction
static u16 aram_head = 0;

// PPC hardware TLB invalidate. The eieio/tlbsync/eieio sequence around
// the tlbie guarantees the invalidation has been observed broadcast-wide
// before continuing, which matters given page state is mutated from two
// threads (game + pager).
static __inline__ void tlbie(void* p)
{
	asm volatile(
        "tlbie %0\n"
        "eieio\n"
        "tlbsync\n"
        "eieio\n"
        :: "r"(p)
    );
}

// Second-chance/clock scan over `phys_map` to pick a MEM1 physical frame
// to reuse: walks forward from `pmap_head`, skipping locked/invalid
// slots, clearing the hardware Referenced/Changed
// bits it finds set (giving a page a "second chance" before eviction), and
// returning the first slot whose PTE has neither bit set. The caller
// (`vm_dsi_handler`) is responsible for actually flushing/evicting it.
//
// The `for(;;++head)` scan has no exit condition beyond finding a valid,
// unlocked, non-referenced frame - it depends entirely on VM_Clear()
// having already populated `phys_map` with a full set of valid frame
// descriptors at startup (see the note there); if that seeding were ever
// skipped or incomplete, this would spin indefinitely on the first fault.
static u16 locate_oldest(void)
{
	u16 head = pmap_head;

	for(;;++head)
	{
		PTE *p;

		if (head >= pmap_max)
			head = 0;

		if (!phys_map[head].valid || phys_map[head].locked)
			continue;

		p = HTABORG+phys_map[head].pte_index;
		tlbie((void*)(VM_Base+phys_map[head].page_index));

		if (p->C)
		{
			p->C = 0;
			phys_map[head].dirty = 1;
			continue;
		}

		if (p->R)
		{
			p->R = 0;
			continue;
		}

		p->data[0] = 0;
		p->data[1] = 0;

		pmap_head = head+1;
		return head;
	}
}

// Writes one PTE into the given PTEG (primary or secondary hash bucket),
// returning a pointer to the slot used, or NULL if all 8 ways in that
// PTEG are already occupied (caller then tries the other hash).
static PTE* StorePTE(PTEG pteg, u32 virtualmem, u32 physical, u8 WIMG, u8 PP, int secondary)
{
	int i;
	PTE p = {{0}};

	p.valid = 1;
	p.VSID = VM_VSID;
	p.hash = secondary ? 1:0;
	p.API = virtualmem >> 22;
	p.RPN = physical >> 12;
	p.WIMG = WIMG;
	p.PP = PP;

	for (i=0; i < 8; i++)
	{
		if (pteg[i].valid)
			continue;

		tlbie((void*)(virtualmem));
		pteg[i].data[1] = p.data[1];
		pteg[i].data[0] = p.data[0];
		return pteg+i;
	}

	return NULL;
}

// Computes the physical address of the primary or secondary PTEG (page
// table entry group / hash bucket) for a given virtual address, per the
// standard PowerPC hashed-page-table addressing scheme.
static PTEG CalcPTEG(u32 virtualmem, int secondary)
{
	uint32_t segment_index = (virtualmem >> 12) & 0xFFFF;
	u32 ptr = MEM_VIRTUAL_TO_PHYSICAL(HTABORG);
	u32 hash = segment_index ^ VM_VSID;

	if (secondary) hash = ~hash;

	hash &= (HTABMASK << 10) | 0x3FF;
	ptr |= hash << 6;

	return (PTEG)MEM_PHYSICAL_TO_K0(ptr);
}

// Maps a virtual page index to a physical MEM1 address by inserting a PTE
// into its primary hash bucket, falling back to the secondary bucket if
// the primary is full.
static PTE* insert_pte(u16 index, u32 physical, u8 WIMG, u8 PP)
{
	PTE *pte;
	int i;
	u32 virtualmem = (u32)(VM_Base+index);

	for (i=0; i < 2; i++)
	{
		PTEG pteg = CalcPTEG(virtualmem, i);
		pte = StorePTE(pteg, virtualmem, physical, WIMG, PP, i);
		if (pte)
			return pte;
	}

	return NULL;
}

// Invalidates the entire TLB (64 congruence classes) - used once at
// (re)initialization time before the hashed page table is rebuilt.
static void tlbia(void)
{
	int i;
	for (i=0; i < 64; i++)
		tlbie((void*)(i*PAGE_SIZE));
}

#ifdef __cplusplus
extern "C" {
#endif

/* This definition is wrong, pHndl does not take frame_context* as a parameter,
 * it has to adjust the stack pointer and finish filling frame_context itself
 */
void __exception_sethandler(u32 nExcept, void (*pHndl)(frame_context*));
extern void default_exceptionhandler(frame_context*);
// use our own exception stub because libogc stupidly requires it
extern void vm_dsi_handler_stub(frame_context*);

#ifdef __cplusplus
}
#endif

// Severs a v_index's association with a MEM1 physical frame (if it still
// has one) and clears its PTE/TLB entry, then marks it not-committed.
//
// Called only when an ARAM cache slot is about to be stolen from an
// `old_occupant` v_index to hold a different, newly-evicted page's data
// (see the ARAM eviction block in vm_dsi_handler below). Because that
// occupant's ARAM-resident copy is about to be overwritten, it can no
// longer be treated as "committed" via ARAM; clearing any MEM1 mapping
// it might still independently hold too (defensive - normally it won't,
// since eviction already set its p_map_index to pmap_max) guarantees the
// next access re-fetches clean data from the SD ROM file rather than
// reading stale/inconsistent state from either tier.
static void ClearMEM1Mapping(u16 v_index) {
	u16 p_index = virt_map[v_index].p_map_index;
	if (p_index != pmap_max) {
		PTE *p = HTABORG + phys_map[p_index].pte_index;
		p->data[0] = 0;
		p->data[1] = 0;
		tlbie((void*)(VM_Base + v_index));

		virt_map[v_index].p_map_index = pmap_max;
		phys_map[p_index].dirty = 0; // Prevent locator from flushing bad data
	}
	virt_map[v_index].committed = 0;
}

// Resets all VM/ARAM/PTE state back to a freshly-initialized condition
// without tearing down and reallocating MEM_Base or the mutex, so a
// different ROM can be loaded mid-session: MEM1 is zeroed, ARAM is
// cleared and its two indirection tables (`aram_map`/`v_to_aram`) and
// FIFO hand (`aram_head`) are reset alongside the PTE/phys_map/virt_map
// rebuild, and the hashed page table is repopulated. Called both from
// VM_Init (initial bring-up) and from VMPager_LoadROM (loading a new ROM
// into an already-running VM).
//
// The loop below unconditionally maps the first `pmap_max` virtual page
// indices 1:1 onto every physical MEM1 frame (skipping only the frames
// occupied by the PTE hash table itself, HTABORG) and inserts a valid PTE
// for each one immediately - this is not an optional preload step. Every
// entry `locate_oldest()` is able to select from is drawn from
// `phys_map[head].valid`, and that scan has no fallback for finding zero
// valid entries: `for(;;++head)` simply spins forever if nothing is ever
// marked valid. Seeding phys_map with a full set of valid, unlocked
// physical-frame descriptors here is therefore a hard prerequisite for
// `locate_oldest()` (and hence every later page fault) to terminate at
// all, not merely a performance nicety. Note that mapping a page this way
// only marks it *present* (a real PTE exists, so the CPU can read/write
// it without ever taking a DSI fault) - it says nothing about its
// *content*: `virt_map[v_index].committed` is explicitly left at 0 for
// all of them, so whatever real data (if any) ends up there depends
// entirely on something else - typically VMPager_LoadROM's synchronous
// preload memcpy - writing into that range afterward.
void VM_Clear(void) {
	if (!vm_initialized) return;

	LWP_MutexLock(vm_mutex);

	memset(MEM_Base, 0, MEMSize);
	AR_Clear(AR_ARAMINTUSER);

	for (u32 j = 0; j < ARAM_MAX_SLOTS; j++)
		aram_map[j] = 0xFFFF;

	for (u32 j = 0; j < 65536; j++)
		v_to_aram[j] = 0xFFFF;

	aram_head = 0;

	tlbia();
	DCZeroRange(MEM_Base, MEMSize);
	HTABORG = (PTE*)(((u32)MEM_Base+0xFFFF)&~0xFFFF);

	// Mandatory phys_map/PTE seeding for locate_oldest() - see the
	// VM_Clear function comment above. Not an optional preload.
	// map pmap_max pages to fill PTEs with valid RPNs
	u32 i;
	u16 index, v_index;

	for (index=0,v_index=0; index<pmap_max; ++index,++v_index)
	{
		if ((PTE*)(MEM_Base+index) == HTABORG)
		{
			for (i=0; i<(PTE_SIZE/PAGE_SIZE); ++i,++index)
				phys_map[index].valid = 0;

			--index;
			--v_index;
			continue;
		}

		phys_map[index].valid = 1;
		phys_map[index].locked = 0;
		phys_map[index].dirty = 0;
		phys_map[index].page_index = v_index;
		phys_map[index].pte_index = insert_pte(v_index, MEM_VIRTUAL_TO_PHYSICAL(MEM_Base+index), 0, 0b10) - HTABORG;
		virt_map[v_index].committed = 0;
		virt_map[v_index].p_map_index = index;
	}

	// all indexes up to 65536
	for (; v_index; ++v_index)
	{
		virt_map[v_index].committed = 0;
		virt_map[v_index].p_map_index = pmap_max;
	}

	pmap_head = 0;
	LWP_MutexUnlock(vm_mutex);
}

// One-time setup: validates the requested VM/physical sizes, reserves the
// VM_Base virtual window at 0x70000000, allocates the real MEM1 backing
// store (MEM_Base, sized for MEMSize + the PTE hash table), brings up
// ARAM (the Tier-1 write-back cache) and the ARQ (async ARAM queue)
// subsystem, then delegates state population to VM_Clear() so the same
// reset logic can be re-run later by VMPager_LoadROM(). Finally wires up
// SDR1/segment registers and installs the DSI handler stub.
void* VM_Init(u32 reqVMSize, u32 reqMEMSize)
{
	if (vm_initialized)
		return VM_Base;

	// parameter checking
	if (reqVMSize>MAX_VM_SIZE || reqMEMSize<MIN_MEM_SIZE || reqMEMSize>MAX_MEM_SIZE || reqVMSize <= reqMEMSize)
	{
		errno = EINVAL;
		return NULL;
	}

	VMSize = (reqVMSize+PAGE_SIZE-1)&PAGE_MASK;
	MEMSize = (reqMEMSize+PAGE_SIZE-1)&PAGE_MASK;
	VM_Base = (vm_page*)(0x70000000);
	pmap_max = MEMSize / PAGE_SIZE + 16;

	if (LWP_MutexInit(&vm_mutex, 0) != 0)
	{
		errno = ENOLCK;
		return NULL;
	}

	MEMSize += PTE_SIZE;
	MEM_Base = (vm_page*)memalign(PAGE_SIZE, MEMSize);

	if (MEM_Base==NULL)
	{
		errno = ENOMEM;
		return NULL;
	}
	
	AR_Init(NULL, 0);
	ARQ_Init();

	vm_initialized = 1;
	VM_Clear();

	// set SDR1
	mtspr(25, MEM_VIRTUAL_TO_PHYSICAL(HTABORG)|HTABMASK);
	// enable SR
	asm volatile("mtsrin %0,%1" :: "r"(VM_VSID), "r"(VM_Base));
	// hook DSI
	__exception_sethandler(EX_DSI, vm_dsi_handler_stub);

	atexit(VM_Deinit);

	return VM_Base;
}

// Returns whether a virtual page's authoritative content has actually
// been loaded from the SD ROM file yet (see the "committed" redefinition
// note in the file header). Used by the pager thread to skip re-fetching
// a page that's already been serviced by an earlier, wider read-ahead.
bool VM_IsCommitted(u16 v_index)
{
	return virt_map[v_index].committed == 1;
}

// Publishes "this page's data is valid" to any thread (typically the
// game thread, spin-waiting in vm_dsi_handler) polling VM_IsCommitted /
// virt_map[].committed for this v_index. Called by the pager thread once
// its fread()+memcpy() for the page has actually completed.
void VM_SetCommitted(u16 v_index) {
	LWP_MutexLock(vm_mutex);
	virt_map[v_index].committed = 1;
	LWP_MutexUnlock(vm_mutex);
}

void VM_Deinit(void)
{
	if (!vm_initialized)
		return;

	// disable SR
	asm volatile("mtsrin %0,%1" :: "r"(0x80000000), "r"(VM_Base));
	// restore default DSI handler
	__exception_sethandler(EX_DSI, default_exceptionhandler);

	free(MEM_Base);
	MEM_Base = NULL;

	if (vm_mutex != LWP_MUTEX_NULL)
	{
		LWP_MutexDestroy(vm_mutex);
		vm_mutex = LWP_MUTEX_NULL;
	}

	vm_initialized = 0;
}

static ARQRequest arq_request;

// The DSI (Data Storage Interrupt) fault handler - the heart of the
// software MMU. Invoked (via vm_dsi_handler_stub) whenever the CPU
// touches a page inside the VM_Base segment that has no valid PTE yet.
//
// Flow:
//   1. Sanity-check the fault actually belongs to this VM segment.
//   2. If the page has never been loaded from the SD ROM file
//      (`!virt_map[v_index].committed`) AND we're not already on the
//      pager thread, hand the request off to the pager thread
//      (`VMPager_RequestPage`) and spin-yield until it reports the page
//      committed - see the "GAME THREAD vs PAGER THREAD" note in the file
//      header for why the pager thread itself must skip this branch.
//   3. Otherwise (page already committed at least once, or we *are* the
//      pager thread faulting in a scratch frame for its own memcpy),
//      resolve the fault directly:
//        a. Pick a MEM1 physical frame to reuse via locate_oldest().
//        b. If that frame is dirty, evict it - write it back into ARAM,
//           allocating/recycling a Tier-1 slot via aram_map/v_to_aram/
//           aram_head (see the header note on ARAM slot recycling and
//           ClearMEM1Mapping).
//        c. Populate the freed frame: DMA back in from ARAM if this
//           v_index already has a live ARAM copy, otherwise zero-fill it
//           (the pager thread's memcpy will overwrite it with real data
//           immediately afterward on the committed=0 path).
//        d. Map the frame to v_index's virtual address via insert_pte.
int vm_dsi_handler(u32 DSISR, u32 DAR)
{
	u16 v_index;
	u16 p_index;

	if (DAR<(u32)VM_Base || DAR>=0x80000000)
		return 0;
	if ((DSISR&~0x02000000)!=0x40000000)
		return 0;
	if (!vm_initialized)
		return 0;

	DAR &= ~0xFFF;
	v_index = (vm_page*)DAR - VM_Base;

	if (!virt_map[v_index].committed) {
		// If this is the game thread during normal gameplay, request the page and wait.
		// If it is the pager thread doing a memcpy OR the main thread doing an initial preload, bypass this and fault in a blank page!
		if (LWP_GetSelf() != VMPager_GetThread() && !VMPager_IsPreloading()) {

			u32 msr;
			asm volatile("mfmsr %0" : "=r"(msr));
			asm volatile("mtmsr %0" :: "r"(msr | MSR_EE)); // Must enable before queue block

			VMPager_RequestPage(v_index);
			while (!virt_map[v_index].committed) {
				LWP_YieldThread();
				asm volatile("" ::: "memory");
			}

			asm volatile("mtmsr %0" :: "r"(msr));
			return 1;
		}
	}

	LWP_MutexLock(vm_mutex);
	p_index = locate_oldest();

	// Evict dirty MEM1 page back to ARAM (L2 Cache Management)
	if (phys_map[p_index].dirty) {
		u16 evict_v_index = phys_map[p_index].page_index;

		// Find an existing ARAM slot or allocate the oldest frame
		u16 target_slot = v_to_aram[evict_v_index];
		if (target_slot == 0xFFFF) {
			target_slot = aram_head;
			aram_head = (aram_head + 1) % ARAM_MAX_SLOTS;

			// If we are stealing a slot, invalidate the old L2 occupant
			u16 old_occupant = aram_map[target_slot];
			if (old_occupant != 0xFFFF) {
				v_to_aram[old_occupant] = 0xFFFF;
				ClearMEM1Mapping(old_occupant);
			}

			aram_map[target_slot] = evict_v_index;
			v_to_aram[evict_v_index] = target_slot;
		}

		u32 aram_offset = target_slot * PAGE_SIZE;

		DCFlushRange(MEM_Base+p_index, PAGE_SIZE);
		AR_StartDMA(AR_MRAMTOARAM, (u32)(MEM_Base+p_index), aram_offset, PAGE_SIZE);
		while(AR_GetDMAStatus());

		virt_map[evict_v_index].committed = 1;
		virt_map[evict_v_index].p_map_index = pmap_max;
		phys_map[p_index].dirty = 0;
	}

	// Fetch v_index if it has been previously committed to ARAM
	if (virt_map[v_index].committed && v_to_aram[v_index] != 0xFFFF)
	{
		u32 aram_offset = v_to_aram[v_index] * PAGE_SIZE;
		DCInvalidateRange(MEM_Base+p_index, PAGE_SIZE);
		AR_StartDMA(AR_ARAMTOMRAM, (u32)(MEM_Base+p_index), aram_offset, PAGE_SIZE);
		while(AR_GetDMAStatus());
	}
	else {
		DCZeroRange(MEM_Base+p_index, PAGE_SIZE);
	}

	// Map new physical page to virtual memory
	virt_map[v_index].p_map_index = p_index;
	phys_map[p_index].page_index = v_index;
	phys_map[p_index].pte_index = insert_pte(v_index, MEM_VIRTUAL_TO_PHYSICAL(MEM_Base+p_index), 0, 0b10) - HTABORG;

	LWP_MutexUnlock(vm_mutex);
	return 1;
}
#endif

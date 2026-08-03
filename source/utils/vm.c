/**
 * vm.c - Implements Virtual Memory for GC
 * Copyright (C) 2012  tueidj
 * ISFS code replaced with ARAM code by emu_kidid
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

static p_map phys_map[2048+(PTE_SIZE/PAGE_SIZE)];
static vm_map virt_map[65536];
static u16 pmap_max, pmap_head;

static PTE* HTABORG;
static vm_page* VM_Base;
static vm_page* MEM_Base = NULL;

static mutex_t vm_mutex = LWP_MUTEX_NULL;
static u32 VMSize = 0;
static u32 MEMSize = 0;
static bool vm_initialized = 0;

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

void VM_Clear(void) {
	if (!vm_initialized) return;

	LWP_MutexLock(vm_mutex);

	memset(MEM_Base, 0, MEMSize);
	AR_Clear(AR_ARAMINTUSER);

	tlbia();
	DCZeroRange(MEM_Base, MEMSize);
	HTABORG = (PTE*)(((u32)MEM_Base+0xFFFF)&~0xFFFF);

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
	VM_Base = (vm_page*)(0x7F000000);
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

void* VM_GetBase(void)
{
	return VM_Base;
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
#ifdef DSISR
#undef DSISR
#endif
#ifdef DAR
#undef DAR
#endif
static ARQRequest arq_request;
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

	LWP_MutexLock(vm_mutex);

	DAR &= ~0xFFF;
	v_index = (vm_page*)DAR - VM_Base;

	p_index = locate_oldest();

	// purge p_index if it's dirty
	if (phys_map[p_index].dirty)
	{
		DCFlushRange(MEM_Base+p_index, PAGE_SIZE);
		ARQ_PostRequest(&arq_request,99,ARQ_MRAMTOARAM,ARQ_PRIO_HI,phys_map[p_index].page_index*PAGE_SIZE,(u32)(MEM_Base+p_index),PAGE_SIZE);
		
		virt_map[phys_map[p_index].page_index].committed = 1;
		virt_map[phys_map[p_index].page_index].p_map_index = pmap_max;
		phys_map[p_index].dirty = 0;
	}

	// fetch v_index if it has been previously committed
	if (virt_map[v_index].committed)
	{
		DCInvalidateRange(MEM_Base+p_index, PAGE_SIZE);
		ARQ_PostRequest(&arq_request,99,ARQ_ARAMTOMRAM,ARQ_PRIO_HI,v_index*PAGE_SIZE,(u32)(MEM_Base+p_index),PAGE_SIZE);
	}
	else
		DCZeroRange(MEM_Base+p_index, PAGE_SIZE);

	virt_map[v_index].p_map_index = p_index;
	phys_map[p_index].page_index = v_index;
	phys_map[p_index].pte_index = insert_pte(v_index, MEM_VIRTUAL_TO_PHYSICAL(MEM_Base+p_index), 0, 0b10) - HTABORG;

	LWP_MutexUnlock(vm_mutex);

	return 1;
}
#endif

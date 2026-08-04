/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 *
 * vmpager.c
 *
 * GameBoy Advance Hardware Virtual Memory Paging (ARAM MMU)
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

extern int GBAROMSize;
extern u8 *rom;

static FILE* romfile = NULL;
static lwp_t pager_thread = LWP_THREAD_NULL;
static mqbox_t pager_queue = MQ_BOX_NULL;
static bool pager_running = false;

#define PREFETCH_PAGES 16
#define PAGE_SIZE 4096
#define PAGE_BUFFER_SIZE (PREFETCH_PAGES * PAGE_SIZE)
#define PAGER_STACK_SIZE (64 * 1024)

static u8* pager_stack = NULL;
static u8* page_buffer = NULL;

lwp_t VMPager_GetThread(void) {
	return pager_thread;
}

static void* VMPager_ThreadFunc(void* arg) {
	while (pager_running) {
		mqmsg_t msg;

		if (MQ_Receive(pager_queue, &msg, MQ_MSG_BLOCK) == FALSE) continue;

		if ((s32)(u32)msg == -1 || !pager_running) break; // Shutdown signal

		u16 req_v_index = (u16)(u32)msg;
		if (VM_IsCommitted(req_v_index)) continue;

		// Align the fetch index to our prefetch block size to avoid overlapping reads
		u16 start_v_index = (req_v_index / PREFETCH_PAGES) * PREFETCH_PAGES;
		u16 max_pages = (GBAROMSize + PAGE_SIZE - 1) / PAGE_SIZE;

		u16 end_v_index = start_v_index + PREFETCH_PAGES;
		if (end_v_index > max_pages) end_v_index = max_pages;

		u32 offset = start_v_index * PAGE_SIZE;
		u32 readSize = (end_v_index - start_v_index) * PAGE_SIZE;
		if (offset + readSize > (u32)GBAROMSize) readSize = GBAROMSize - offset;

		if (romfile) {
			fseeko(romfile, offset, SEEK_SET);
			fread(page_buffer, 1, readSize, romfile);
			// This memcpy will intentionally trigger DSI exceptions on the pager thread
			memcpy(rom + offset, page_buffer, readSize);
			// Explicitly mark these pages as committed now that MEM1 is populated
			u32 pages_read = (readSize + PAGE_SIZE - 1) / PAGE_SIZE;
			for (u32 i = 0; i < pages_read; i++) {
				VM_SetCommitted(start_v_index + i);
			}
		}
	}
	return NULL;
}

void VMPager_Init() {
	if (pager_running) return;

	pager_stack = (u8*)memalign(32, PAGER_STACK_SIZE);
	page_buffer = (u8*)memalign(32, PAGE_BUFFER_SIZE);

	MQ_Init(&pager_queue, 128);

	pager_running = true;
	LWP_CreateThread(&pager_thread, VMPager_ThreadFunc, NULL, pager_stack, PAGER_STACK_SIZE, 80);
}

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

void VMPager_RequestPage(u16 v_index) {
	if (pager_running && pager_queue != MQ_BOX_NULL) {
		MQ_Send(pager_queue, (mqmsg_t)(u32)v_index, MQ_MSG_BLOCK);
	}
}

int VMPager_LoadROM(const char * filepath) {
	VMPager_CloseFile();
	
	romfile = fopen(filepath, "rb");
	if (romfile == NULL) {
		return 0;
	}

	fseeko(romfile, 0, SEEK_END);
	GBAROMSize = ftello(romfile);
	fseeko(romfile, 0, SEEK_SET);

	rom = (u8*)VM_GetBase();

	u32 offset = 0;
	// Only preload up to 16MB (ARAM limit) at boot
	u32 preloadSize = (GBAROMSize > ARAM_SIZE) ? (ARAM_SIZE) : GBAROMSize;

	while (offset < preloadSize) {
		u32 readSize = preloadSize - offset;
		if (readSize > PAGE_BUFFER_SIZE) readSize = PAGE_BUFFER_SIZE;

		fread(page_buffer, 1, readSize, romfile);
		memcpy(rom + offset, page_buffer, readSize);
		offset += readSize;
	}

	u32 pages_read = (preloadSize + PAGE_SIZE - 1) / PAGE_SIZE;
	for (u32 i = 0; i < pages_read; i++) {
		VM_SetCommitted(i);
	}

	return GBAROMSize;
}

void VMPager_CloseFile() {
	if (romfile) {
		fclose(romfile);
		romfile = NULL;
	}
	rom = NULL;
	VM_Clear();
}
#endif

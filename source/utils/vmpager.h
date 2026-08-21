/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth September 2008
 *
 * vmpager.h
 *
 * GameBoy Advance Virtual Memory Paging
 ***************************************************************************/

#ifndef _VMPAGER_H_
#define _VMPAGER_H_

#ifdef HW_DOL

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void VMPager_Init(u8 *vmPtr);
void VMPager_Shutdown();
void VMPager_RequestAndWaitPage(u16 v_index);
void VMPager_StartPreload(FILE* file, u32 size);
void VMPager_CommitPageRange(u32 start_page, u32 end_page);
void VMPager_CompletePreload();
lwp_t VMPager_GetThread(void);
bool VMPager_IsPreloading(void);
void VMPager_CloseFile();
#ifdef __cplusplus
}
#endif

#endif

#endif

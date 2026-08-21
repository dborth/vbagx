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

#ifdef __cplusplus
extern "C" {
#endif

void VMPager_Init(u8 *vmPtr);
void VMPager_Shutdown();
void VMPager_RequestAndWaitPage(u16 v_index);
int VMPager_LoadROM(const char * filepath);
lwp_t VMPager_GetThread(void);
bool VMPager_IsPreloading(void);
void VMPager_CloseFile();
#ifdef __cplusplus
}
#endif

#endif

#endif

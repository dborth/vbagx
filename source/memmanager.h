/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * memmanager.h
 *
 * Memory manager
 ***************************************************************************/

#ifndef _MEMMANAGER_H_
#define _MEMMANAGER_H_

#include <gctypes.h>

#define IMAGE_BUFFER_SIZE (640 * 480 * 4)

#ifdef __cplusplus
extern "C" {
#endif

extern u8* romPtr;

void InitMemManager();
void SwitchMemoryModeMenu();
void SwitchMemoryModeGame();
void* mem1_malloc(u32 size);
void mem1_free(void *ptr);
int mem1_size_free();
char* mem1_strdup(const char *s);
void* extmem_malloc(u32 size);
void extmem_free(void *ptr);
int extmem_size_free();

#ifdef __cplusplus
}
#endif

#endif

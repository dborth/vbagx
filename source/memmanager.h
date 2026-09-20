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

#define IMAGE_BUFFER_SIZE (640 * 480 * 4)
#define IMAGE_DECODE_SCRATCH_SIZE (IMAGE_BUFFER_SIZE + (480 * sizeof(void*)))
#define PNG_FILE_BUFFER_SIZE (512 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t* romPtr;

#ifdef __WIIU__
void InitJitWiiU();
#endif

// True if this platform can run the GBA JIT at all
bool JitIsAvailable();

// Clears EmuSettings.dynamicRecompilation if JIT is not available
void EnforceJitSetting();

void EnforceJitSettingForGame();

void InitMemManager();
void SwitchMemoryModeMenu();
void SwitchMemoryModeGame();
void* memspace_malloc(uint32_t size);
void memspace_free(void *ptr);
int memspace_size_free();
char* memspace_strdup(const char *s);
void* extmem_malloc(uint32_t size);
void extmem_free(void *ptr);
int extmem_size_free();

#ifdef __cplusplus
}
#endif

#endif

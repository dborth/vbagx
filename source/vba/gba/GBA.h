#ifndef GBA_H
#define GBA_H

#include "../System.h"

#define SAVE_GAME_VERSION_1 1
#define SAVE_GAME_VERSION_2 2
#define SAVE_GAME_VERSION_3 3
#define SAVE_GAME_VERSION_4 4
#define SAVE_GAME_VERSION_5 5
#define SAVE_GAME_VERSION_6 6
#define SAVE_GAME_VERSION_7 7
#define SAVE_GAME_VERSION_8 8
#define SAVE_GAME_VERSION_9 9
#define SAVE_GAME_VERSION_10 10
#define SAVE_GAME_VERSION  SAVE_GAME_VERSION_10

typedef struct {
  u8 *address;
  u32 mask;
} memoryMap;

typedef struct __attribute__((aligned(32))) {
    u8*  readPages[256];
    u32  readMasks[256];
} GBAReadPageTable;

extern GBAReadPageTable gbaReadTable;
extern u8* gbaWritePagePtrs[256];

typedef union {
  struct {
    u8 B3;
    u8 B2;
    u8 B1;
    u8 B0;
  } B;
  struct {
    u16 W1;
    u16 W0;
  } W;
  u32 I;
} reg_pair;

struct CPUFlags {
    u32 N;
    u32 Z;
    u32 C;
    u32 V;
} __attribute__((aligned(32)));

// used strictly for load/save statesave
struct BoolCPUFlags {
	bool N;
	bool Z;
	bool C;
	bool V;
};

extern memoryMap map[256];
extern reg_pair reg[45];
extern u8 biosProtected[4];

extern CPUFlags gbaFlags;
extern bool armIrqEnable;
extern bool armState;
extern int armMode;
extern void (*cpuSaveGameFunc)(u32,u8);

extern void CPUCleanUp();
extern void CPUUpdateRender();
extern void CPUUpdateRenderBuffers(bool);
extern bool CPUReadMemState(char *, int);
extern bool CPUWriteMemState(char *, int);
extern void doMirroring(bool);
extern void CPUUpdateRegister(u32, u16);
extern void applyTimer ();
extern void CPUInit(const char *,bool);
extern void CPUReset();
extern void CPULoop(int);
extern void CPUCheckDMA(int,int);

extern struct EmulatedSystem GBASystem;

#define R13_IRQ  18
#define R14_IRQ  19
#define SPSR_IRQ 20
#define R13_USR  26
#define R14_USR  27
#define R13_SVC  28
#define R14_SVC  29
#define SPSR_SVC 30
#define R13_ABT  31
#define R14_ABT  32
#define SPSR_ABT 33
#define R13_UND  34
#define R14_UND  35
#define SPSR_UND 36
#define R8_FIQ   37
#define R9_FIQ   38
#define R10_FIQ  39
#define R11_FIQ  40
#define R12_FIQ  41
#define R13_FIQ  42
#define R14_FIQ  43
#define SPSR_FIQ 44

#include "Cheats.h"
#include "Globals.h"
#include "EEprom.h"
#include "Flash.h"

#endif // GBA_H

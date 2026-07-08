#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "GBA.h"
#include "GBAcpu.h"
#include "GBAinline.h"
#include "Globals.h"
#include "GBAGfx.h"
#include "EEprom.h"
#include "Flash.h"
#include "Sound.h"
#include "Sram.h"
#include "bios.h"
#include "Cheats.h"
#include "../NLS.h"
#include "../Util.h"
#include "../common/Port.h"
#include "../System.h"

#define strcasecmp strcasecmp

extern int emulating;

int SWITicks = 0;
int IRQTicks = 0;

u32 mastercode = 0;
int layerEnableDelay = 0;
bool busPrefetch = false;
bool busPrefetchEnable = false;
u32 busPrefetchCount = 0;
int cpuDmaTicksToUpdate = 0;
int cpuDmaCount = 0;
bool cpuDmaHack = false;
u32 cpuDmaLast = 0;
int dummyAddress = 0;

bool cpuBreakLoop = false;
int cpuNextEvent = 0;

int gbaSaveType = 0; // used to remember the save type on reset
bool intState = false;
bool stopState = false;
bool holdState = false;
int holdType = 0;
bool cpuSramEnabled = true;
bool cpuFlashEnabled = true;
bool cpuEEPROMEnabled = true;
bool cpuEEPROMSensorEnabled = false;

u32 cpuPrefetch[2];

int cpuTotalTicks = 0;

int lcdTicks = (useBios && !skipBios) ? 1008 : 208;
u8 timerOnOffDelay = 0;
u16 timer0Value = 0;
bool timer0On = false;
int timer0Ticks = 0;
int timer0Reload = 0;
int timer0ClockReload  = 0;
u16 timer1Value = 0;
bool timer1On = false;
int timer1Ticks = 0;
int timer1Reload = 0;
int timer1ClockReload  = 0;
u16 timer2Value = 0;
bool timer2On = false;
int timer2Ticks = 0;
int timer2Reload = 0;
int timer2ClockReload  = 0;
u16 timer3Value = 0;
bool timer3On = false;
int timer3Ticks = 0;
int timer3Reload = 0;
int timer3ClockReload  = 0;
u32 dma0Source = 0;
u32 dma0Dest = 0;
u32 dma1Source = 0;
u32 dma1Dest = 0;
u32 dma2Source = 0;
u32 dma2Dest = 0;
u32 dma3Source = 0;
u32 dma3Dest = 0;
void (*cpuSaveGameFunc)(u32,u8) = flashSaveDecide;
void (*renderLine)() = mode0RenderLine;
bool fxOn = false;
bool windowOn = false;
int frameCount = 0;
char buffer[1024];
u32 lastTime = 0;
int count = 0;

int capture = 0;
int capturePrevious = 0;
int captureNumber = 0;

int armOpcodeCount = 0;
int thumbOpcodeCount = 0;

const int TIMER_TICKS[4] = {
  0,
  6,
  8,
  10
};

const u32  objTilesAddress [3] = {0x010000, 0x014000, 0x014000};
const u8 gamepakRamWaitState[4] = { 4, 3, 2, 8 };
const u8 gamepakWaitState[4] =  { 4, 3, 2, 8 };
const u8 gamepakWaitState0[2] = { 2, 1 };
const u8 gamepakWaitState1[2] = { 4, 1 };
const u8 gamepakWaitState2[2] = { 8, 1 };
const bool isInRom [16]=
  { false, false, false, false, false, false, false, false,
    true, true, true, true, true, true, false, false };

u8 memoryWait[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 0 };
u8 memoryWait32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 7, 7, 9, 9, 13, 13, 4, 0 };
u8 memoryWaitSeq[16] =
  { 0, 0, 2, 0, 0, 0, 0, 0, 2, 2, 4, 4, 8, 8, 4, 0 };
u8 memoryWaitSeq32[16] =
  { 0, 0, 5, 0, 0, 1, 1, 0, 5, 5, 9, 9, 17, 17, 4, 0 };

// The videoMemoryWait constants are used to add some waitstates
// if the opcode access video memory data outside of vblank/hblank
// It seems to happen on only one ticks for each pixel.
// Not used for now (too problematic with current code).
//const u8 videoMemoryWait[16] =
//  {0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};


u8 biosProtected[4];

bool cpuBiosSwapped = false;

u32 myROM[] = {
0xEA000006,
0xEA000093,
0xEA000006,
0x00000000,
0x00000000,
0x00000000,
0xEA000088,
0x00000000,
0xE3A00302,
0xE1A0F000,
0xE92D5800,
0xE55EC002,
0xE28FB03C,
0xE79BC10C,
0xE14FB000,
0xE92D0800,
0xE20BB080,
0xE38BB01F,
0xE129F00B,
0xE92D4004,
0xE1A0E00F,
0xE12FFF1C,
0xE8BD4004,
0xE3A0C0D3,
0xE129F00C,
0xE8BD0800,
0xE169F00B,
0xE8BD5800,
0xE1B0F00E,
0x0000009C,
0x0000009C,
0x0000009C,
0x0000009C,
0x000001F8,
0x000001F0,
0x000000AC,
0x000000A0,
0x000000FC,
0x00000168,
0xE12FFF1E,
0xE1A03000,
0xE1A00001,
0xE1A01003,
0xE2113102,
0x42611000,
0xE033C040,
0x22600000,
0xE1B02001,
0xE15200A0,
0x91A02082,
0x3AFFFFFC,
0xE1500002,
0xE0A33003,
0x20400002,
0xE1320001,
0x11A020A2,
0x1AFFFFF9,
0xE1A01000,
0xE1A00003,
0xE1B0C08C,
0x22600000,
0x42611000,
0xE12FFF1E,
0xE92D0010,
0xE1A0C000,
0xE3A01001,
0xE1500001,
0x81A000A0,
0x81A01081,
0x8AFFFFFB,
0xE1A0000C,
0xE1A04001,
0xE3A03000,
0xE1A02001,
0xE15200A0,
0x91A02082,
0x3AFFFFFC,
0xE1500002,
0xE0A33003,
0x20400002,
0xE1320001,
0x11A020A2,
0x1AFFFFF9,
0xE0811003,
0xE1B010A1,
0xE1510004,
0x3AFFFFEE,
0xE1A00004,
0xE8BD0010,
0xE12FFF1E,
0xE0010090,
0xE1A01741,
0xE2611000,
0xE3A030A9,
0xE0030391,
0xE1A03743,
0xE2833E39,
0xE0030391,
0xE1A03743,
0xE2833C09,
0xE283301C,
0xE0030391,
0xE1A03743,
0xE2833C0F,
0xE28330B6,
0xE0030391,
0xE1A03743,
0xE2833C16,
0xE28330AA,
0xE0030391,
0xE1A03743,
0xE2833A02,
0xE2833081,
0xE0030391,
0xE1A03743,
0xE2833C36,
0xE2833051,
0xE0030391,
0xE1A03743,
0xE2833CA2,
0xE28330F9,
0xE0000093,
0xE1A00840,
0xE12FFF1E,
0xE3A00001,
0xE3A01001,
0xE92D4010,
0xE3A03000,
0xE3A04001,
0xE3500000,
0x1B000004,
0xE5CC3301,
0xEB000002,
0x0AFFFFFC,
0xE8BD4010,
0xE12FFF1E,
0xE3A0C301,
0xE5CC3208,
0xE15C20B8,
0xE0110002,
0x10222000,
0x114C20B8,
0xE5CC4208,
0xE12FFF1E,
0xE92D500F,
0xE3A00301,
0xE1A0E00F,
0xE510F004,
0xE8BD500F,
0xE25EF004,
0xE59FD044,
0xE92D5000,
0xE14FC000,
0xE10FE000,
0xE92D5000,
0xE3A0C302,
0xE5DCE09C,
0xE35E00A5,
0x1A000004,
0x05DCE0B4,
0x021EE080,
0xE28FE004,
0x159FF018,
0x059FF018,
0xE59FD018,
0xE8BD5000,
0xE169F00C,
0xE8BD5000,
0xE25EF004,
0x03007FF0,
0x09FE2000,
0x09FFC000,
0x03007FE0
};

variable_desc saveGameStruct[] = {
  { &DISPCNT  , sizeof(u16) },
  { &DISPSTAT , sizeof(u16) },
  { &VCOUNT   , sizeof(u16) },
  { &BG0CNT   , sizeof(u16) },
  { &BG1CNT   , sizeof(u16) },
  { &BG2CNT   , sizeof(u16) },
  { &BG3CNT   , sizeof(u16) },
  { &BG0HOFS  , sizeof(u16) },
  { &BG0VOFS  , sizeof(u16) },
  { &BG1HOFS  , sizeof(u16) },
  { &BG1VOFS  , sizeof(u16) },
  { &BG2HOFS  , sizeof(u16) },
  { &BG2VOFS  , sizeof(u16) },
  { &BG3HOFS  , sizeof(u16) },
  { &BG3VOFS  , sizeof(u16) },
  { &BG2PA    , sizeof(u16) },
  { &BG2PB    , sizeof(u16) },
  { &BG2PC    , sizeof(u16) },
  { &BG2PD    , sizeof(u16) },
  { &BG2X_L   , sizeof(u16) },
  { &BG2X_H   , sizeof(u16) },
  { &BG2Y_L   , sizeof(u16) },
  { &BG2Y_H   , sizeof(u16) },
  { &BG3PA    , sizeof(u16) },
  { &BG3PB    , sizeof(u16) },
  { &BG3PC    , sizeof(u16) },
  { &BG3PD    , sizeof(u16) },
  { &BG3X_L   , sizeof(u16) },
  { &BG3X_H   , sizeof(u16) },
  { &BG3Y_L   , sizeof(u16) },
  { &BG3Y_H   , sizeof(u16) },
  { &WIN0H    , sizeof(u16) },
  { &WIN1H    , sizeof(u16) },
  { &WIN0V    , sizeof(u16) },
  { &WIN1V    , sizeof(u16) },
  { &WININ    , sizeof(u16) },
  { &WINOUT   , sizeof(u16) },
  { &MOSAIC   , sizeof(u16) },
  { &BLDMOD   , sizeof(u16) },
  { &COLEV    , sizeof(u16) },
  { &COLY     , sizeof(u16) },
  { &DM0SAD_L , sizeof(u16) },
  { &DM0SAD_H , sizeof(u16) },
  { &DM0DAD_L , sizeof(u16) },
  { &DM0DAD_H , sizeof(u16) },
  { &DM0CNT_L , sizeof(u16) },
  { &DM0CNT_H , sizeof(u16) },
  { &DM1SAD_L , sizeof(u16) },
  { &DM1SAD_H , sizeof(u16) },
  { &DM1DAD_L , sizeof(u16) },
  { &DM1DAD_H , sizeof(u16) },
  { &DM1CNT_L , sizeof(u16) },
  { &DM1CNT_H , sizeof(u16) },
  { &DM2SAD_L , sizeof(u16) },
  { &DM2SAD_H , sizeof(u16) },
  { &DM2DAD_L , sizeof(u16) },
  { &DM2DAD_H , sizeof(u16) },
  { &DM2CNT_L , sizeof(u16) },
  { &DM2CNT_H , sizeof(u16) },
  { &DM3SAD_L , sizeof(u16) },
  { &DM3SAD_H , sizeof(u16) },
  { &DM3DAD_L , sizeof(u16) },
  { &DM3DAD_H , sizeof(u16) },
  { &DM3CNT_L , sizeof(u16) },
  { &DM3CNT_H , sizeof(u16) },
  { &TM0D     , sizeof(u16) },
  { &TM0CNT   , sizeof(u16) },
  { &TM1D     , sizeof(u16) },
  { &TM1CNT   , sizeof(u16) },
  { &TM2D     , sizeof(u16) },
  { &TM2CNT   , sizeof(u16) },
  { &TM3D     , sizeof(u16) },
  { &TM3CNT   , sizeof(u16) },
  { &P1       , sizeof(u16) },
  { &IE       , sizeof(u16) },
  { &IF       , sizeof(u16) },
  { &IME      , sizeof(u16) },
  { &holdState, sizeof(bool) },
  { &holdType, sizeof(int) },
  { &lcdTicks, sizeof(int) },
  { &timer0On , sizeof(bool) },
  { &timer0Ticks , sizeof(int) },
  { &timer0Reload , sizeof(int) },
  { &timer0ClockReload  , sizeof(int) },
  { &timer1On , sizeof(bool) },
  { &timer1Ticks , sizeof(int) },
  { &timer1Reload , sizeof(int) },
  { &timer1ClockReload  , sizeof(int) },
  { &timer2On , sizeof(bool) },
  { &timer2Ticks , sizeof(int) },
  { &timer2Reload , sizeof(int) },
  { &timer2ClockReload  , sizeof(int) },
  { &timer3On , sizeof(bool) },
  { &timer3Ticks , sizeof(int) },
  { &timer3Reload , sizeof(int) },
  { &timer3ClockReload  , sizeof(int) },
  { &dma0Source , sizeof(u32) },
  { &dma0Dest , sizeof(u32) },
  { &dma1Source , sizeof(u32) },
  { &dma1Dest , sizeof(u32) },
  { &dma2Source , sizeof(u32) },
  { &dma2Dest , sizeof(u32) },
  { &dma3Source , sizeof(u32) },
  { &dma3Dest , sizeof(u32) },
  { &fxOn, sizeof(bool) },
  { &windowOn, sizeof(bool) },
  { &N_FLAG , sizeof(bool) },
  { &C_FLAG , sizeof(bool) },
  { &Z_FLAG , sizeof(bool) },
  { &V_FLAG , sizeof(bool) },
  { &armState , sizeof(bool) },
  { &armIrqEnable , sizeof(bool) },
  { &armNextPC , sizeof(u32) },
  { &armMode , sizeof(int) },
  { &saveType , sizeof(int) },
  { NULL, 0 }
};

static int romSize = 0x2000000;

static inline int CPUUpdateTicks()
{
	int cpuLoopTicks = lcdTicks;

	// Branchless minimum macro mapping to Broadway's dual integer pipeline
	#define B_MIN(a, b) ((b) ^ (((a) ^ (b)) & -((a) < (b))))

	cpuLoopTicks = B_MIN(cpuLoopTicks, soundTicks);

	// Mask out inactive timers with INT_MAX (0x7FFFFFFF) so they drop out of the min evaluation
	int t0 = timer0On ? timer0Ticks : 0x7FFFFFFF;
	cpuLoopTicks = B_MIN(cpuLoopTicks, t0);

	int t1 = (timer1On & !(TM1CNT & 4)) ? timer1Ticks : 0x7FFFFFFF;
	cpuLoopTicks = B_MIN(cpuLoopTicks, t1);

	int t2 = (timer2On & !(TM2CNT & 4)) ? timer2Ticks : 0x7FFFFFFF;
	cpuLoopTicks = B_MIN(cpuLoopTicks, t2);

	int t3 = (timer3On & !(TM3CNT & 4)) ? timer3Ticks : 0x7FFFFFFF;
	cpuLoopTicks = B_MIN(cpuLoopTicks, t3);

	int swi = SWITicks ? SWITicks : 0x7FFFFFFF;
	cpuLoopTicks = B_MIN(cpuLoopTicks, swi);

	int irq = IRQTicks ? IRQTicks : 0x7FFFFFFF;
	cpuLoopTicks = B_MIN(cpuLoopTicks, irq);

	#undef B_MIN
	return cpuLoopTicks;
}

void CPUUpdateWindow0()
{
  int x00 = WIN0H>>8;
  int x01 = WIN0H & 255;

  if(x00 <= x01) {
    for(int i = 0; i < 240; ++i) {
      gfxInWin0[i] = (i >= x00 && i < x01);
    }
  } else {
    for(int i = 0; i < 240; ++i) {
      gfxInWin0[i] = (i >= x00 || i < x01);
    }
  }
}

void CPUUpdateWindow1()
{
  int x00 = WIN1H>>8;
  int x01 = WIN1H & 255;

  if(x00 <= x01) {
    for(int i = 0; i < 240; ++i) {
      gfxInWin1[i] = (i >= x00 && i < x01);
    }
  } else {
    for(int i = 0; i < 240; ++i) {
      gfxInWin1[i] = (i >= x00 || i < x01);
    }
  }
}

extern u32 line0[240];
extern u32 line1[240];
extern u32 line2[240];
extern u32 line3[240];

#define CLEAR_ARRAY(a) \
  {\
    u32 *array = (a);\
    for(int i = 0; i < 30; i++) {\
      *array++ = 0x80000000;\
      *array++ = 0x80000000;\
      *array++ = 0x80000000;\
      *array++ = 0x80000000;\
      *array++ = 0x80000000;\
      *array++ = 0x80000000;\
      *array++ = 0x80000000;\
      *array++ = 0x80000000;\
    }\
  }

void CPUUpdateRenderBuffers(bool force)
{
  if(!(layerEnable & 0x0100) || force) {
    CLEAR_ARRAY(line0);
  }
  if(!(layerEnable & 0x0200) || force) {
    CLEAR_ARRAY(line1);
  }
  if(!(layerEnable & 0x0400) || force) {
    CLEAR_ARRAY(line2);
  }
  if(!(layerEnable & 0x0800) || force) {
    CLEAR_ARRAY(line3);
  }
}

static bool CPUWriteState(gzFile gzFile)
{
  utilWriteInt(gzFile, SAVE_GAME_VERSION);

  utilGzWrite(gzFile, &rom[0xa0], 16);

  utilWriteInt(gzFile, useBios);

  utilGzWrite(gzFile, &reg[0], sizeof(reg));

  utilWriteData(gzFile, saveGameStruct);

  // new to version 0.7.1
  utilWriteInt(gzFile, stopState);
  // new to version 0.8
  utilWriteInt(gzFile, IRQTicks);

  utilGzWrite(gzFile, internalRAM, 0x8000);
  utilGzWrite(gzFile, paletteRAM, 0x400);
  utilGzWrite(gzFile, workRAM, 0x40000);
  utilGzWrite(gzFile, vram, 0x20000);
  utilGzWrite(gzFile, oam, 0x400);
  utilGzWrite(gzFile, pix, 4*241*162);
  utilGzWrite(gzFile, ioMem, 0x400);

  eepromSaveGame(gzFile);
  flashSaveGame(gzFile);
  soundSaveGame(gzFile);

  cheatsSaveGame(gzFile);

  // version 1.5
  rtcSaveGame(gzFile);

  return true;
}

bool CPUWriteMemState(char *memory, int available)
{
  gzFile gzFile = utilMemGzOpen(memory, available, "w");

  if(gzFile == NULL) {
    return false;
  }

  bool res = CPUWriteState(gzFile);

  long pos = utilGzMemTell(gzFile)+8;

  if(pos >= (available))
    res = false;

  utilGzClose(gzFile);

  return res;
}

static bool CPUReadState(gzFile gzFile)
{
  int version = utilReadInt(gzFile);

  if(version > SAVE_GAME_VERSION || version < SAVE_GAME_VERSION_1) {
    systemMessage(MSG_UNSUPPORTED_VBA_SGM,
                  N_("Unsupported VisualBoyAdvance save game version %d"),
                  version);
    return false;
  }

  u8 romname[17];

  utilGzRead(gzFile, romname, 16);

  if(memcmp(&rom[0xa0], romname, 16) != 0) {
    romname[16]=0;
    for(int i = 0; i < 16; i++)
      if(romname[i] < 32)
        romname[i] = 32;
    systemMessage(MSG_CANNOT_LOAD_SGM, N_("Cannot load save game for %s"), romname);
    return false;
  }

  bool ub = utilReadInt(gzFile) ? true : false;

  if(ub != useBios) {
    if(useBios)
      systemMessage(MSG_SAVE_GAME_NOT_USING_BIOS,
                    N_("Save game is not using the BIOS files"));
    else
      systemMessage(MSG_SAVE_GAME_USING_BIOS,
                    N_("Save game is using the BIOS file"));
    return false;
  }

  utilGzRead(gzFile, &reg[0], sizeof(reg));

  utilReadData(gzFile, saveGameStruct);

  if(version < SAVE_GAME_VERSION_3)
    stopState = false;
  else
    stopState = utilReadInt(gzFile) ? true : false;

  if(version < SAVE_GAME_VERSION_4)
  {
    IRQTicks = 0;
    intState = false;
  }
  else
  {
    IRQTicks = utilReadInt(gzFile);
    if (IRQTicks>0)
      intState = true;
    else
    {
      intState = false;
      IRQTicks = 0;
    }
  }

  utilGzRead(gzFile, internalRAM, 0x8000);
  utilGzRead(gzFile, paletteRAM, 0x400);
  utilGzRead(gzFile, workRAM, 0x40000);
  utilGzRead(gzFile, vram, 0x20000);
  utilGzRead(gzFile, oam, 0x400);
  if(version < SAVE_GAME_VERSION_6)
    utilGzRead(gzFile, pix, 4*240*160);
  else
    utilGzRead(gzFile, pix, 4*241*162);
  utilGzRead(gzFile, ioMem, 0x400);

  if(skipSaveGameBattery) {
    // skip eeprom data
    eepromReadGameSkip(gzFile, version);
    // skip flash data
    flashReadGameSkip(gzFile, version);
  } else {
    eepromReadGame(gzFile, version);
    flashReadGame(gzFile, version);
  }
  soundReadGame(gzFile, version);

  if(version > SAVE_GAME_VERSION_1) {
    if(skipSaveGameCheats) {
      // skip cheats list data
      cheatsReadGameSkip(gzFile, version);
    } else {
      cheatsReadGame(gzFile, version);
    }
  }
  if(version > SAVE_GAME_VERSION_6) {
    rtcReadGame(gzFile);
  }

  if(version <= SAVE_GAME_VERSION_7) {
    u32 temp;
#define SWAP(a,b,c) \
    temp = (a);\
    (a) = (b)<<16|(c);\
    (b) = (temp) >> 16;\
    (c) = (temp) & 0xFFFF;

    SWAP(dma0Source, DM0SAD_H, DM0SAD_L);
    SWAP(dma0Dest,   DM0DAD_H, DM0DAD_L);
    SWAP(dma1Source, DM1SAD_H, DM1SAD_L);
    SWAP(dma1Dest,   DM1DAD_H, DM1DAD_L);
    SWAP(dma2Source, DM2SAD_H, DM2SAD_L);
    SWAP(dma2Dest,   DM2DAD_H, DM2DAD_L);
    SWAP(dma3Source, DM3SAD_H, DM3SAD_L);
    SWAP(dma3Dest,   DM3DAD_H, DM3DAD_L);
  }

  if(version <= SAVE_GAME_VERSION_8) {
    timer0ClockReload = TIMER_TICKS[TM0CNT & 3];
    timer1ClockReload = TIMER_TICKS[TM1CNT & 3];
    timer2ClockReload = TIMER_TICKS[TM2CNT & 3];
    timer3ClockReload = TIMER_TICKS[TM3CNT & 3];

    timer0Ticks = ((0x10000 - TM0D) << timer0ClockReload) - timer0Ticks;
    timer1Ticks = ((0x10000 - TM1D) << timer1ClockReload) - timer1Ticks;
    timer2Ticks = ((0x10000 - TM2D) << timer2ClockReload) - timer2Ticks;
    timer3Ticks = ((0x10000 - TM3D) << timer3ClockReload) - timer3Ticks;
    interp_rate();
  }

  // set pointers!
  layerEnable = layerSettings & DISPCNT;

  CPUUpdateRender();
  CPUUpdateRenderBuffers(true);
  CPUUpdateWindow0();
  CPUUpdateWindow1();
  gbaSaveType = 0;
  switch(saveType) {
  case 0:
    cpuSaveGameFunc = flashSaveDecide;
    break;
  case 1:
    cpuSaveGameFunc = sramWrite;
    gbaSaveType = 1;
    break;
  case 2:
    cpuSaveGameFunc = flashWrite;
    gbaSaveType = 2;
    break;
  case 3:
     break;
  case 5:
    gbaSaveType = 5;
    break;
  default:
    systemMessage(MSG_UNSUPPORTED_SAVE_TYPE,
                  N_("Unsupported save type %d"), saveType);
    break;
  }
  if(eepromInUse)
    gbaSaveType = 3;

  systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
  if(armState) {
    ARM_PREFETCH;
  } else {
    THUMB_PREFETCH;
  }

  CPUUpdateRegister(0x204, CPUReadHalfWordQuick(0x4000204));

  return true;
}

bool CPUReadMemState(char *memory, int available)
{
  gzFile gzFile = utilMemGzOpen(memory, available, "r");

  bool res = CPUReadState(gzFile);

  utilGzClose(gzFile);

  return res;
}

void CPUCleanUp()
{
  if(rom != NULL) {
    free(rom);
    rom = NULL;
  }

  if(vram != NULL) {
    free(vram);
    vram = NULL;
  }

  if(paletteRAM != NULL) {
    free(paletteRAM);
    paletteRAM = NULL;
  }

  if(internalRAM != NULL) {
    free(internalRAM);
    internalRAM = NULL;
  }

  if(workRAM != NULL) {
    free(workRAM);
    workRAM = NULL;
  }

  if(bios != NULL) {
    free(bios);
    bios = NULL;
  }

  if(pix != NULL) {
    free(pix);
    pix = NULL;
  }

  if(oam != NULL) {
    free(oam);
    oam = NULL;
  }

  if(ioMem != NULL) {
    free(ioMem);
    ioMem = NULL;
  }

  systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

  emulating = 0;
}

void doMirroring (bool b)
{
  u32 mirroredRomSize = (((romSize)>>20) & 0x3F)<<20;
  u32 mirroredRomAddress = romSize;
  if ((mirroredRomSize <=0x800000) && (b))
  {
    mirroredRomAddress = mirroredRomSize;
    if (mirroredRomSize==0)
        mirroredRomSize=0x100000;
    while (mirroredRomAddress<0x01000000)
    {
      memcpy ((u16 *)(rom+mirroredRomAddress), (u16 *)(rom), mirroredRomSize);
      mirroredRomAddress+=mirroredRomSize;
    }
  }
}

void CPUUpdateRender()
{
  switch(DISPCNT & 7) {
  case 0:
    if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
       cpuDisableSfx)
      renderLine = mode0RenderLine;
    else if(fxOn && !windowOn && !(layerEnable & 0x8000))
      renderLine = mode0RenderLineNoWindow;
    else
      renderLine = mode0RenderLineAll;
    break;
  case 1:
    if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
       cpuDisableSfx)
      renderLine = mode1RenderLine;
    else if(fxOn && !windowOn && !(layerEnable & 0x8000))
      renderLine = mode1RenderLineNoWindow;
    else
      renderLine = mode1RenderLineAll;
    break;
  case 2:
    if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
       cpuDisableSfx)
      renderLine = mode2RenderLine;
    else if(fxOn && !windowOn && !(layerEnable & 0x8000))
      renderLine = mode2RenderLineNoWindow;
    else
      renderLine = mode2RenderLineAll;
    break;
  case 3:
    if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
       cpuDisableSfx)
      renderLine = mode3RenderLine;
    else if(fxOn && !windowOn && !(layerEnable & 0x8000))
      renderLine = mode3RenderLineNoWindow;
    else
      renderLine = mode3RenderLineAll;
    break;
  case 4:
    if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
       cpuDisableSfx)
      renderLine = mode4RenderLine;
    else if(fxOn && !windowOn && !(layerEnable & 0x8000))
      renderLine = mode4RenderLineNoWindow;
    else
      renderLine = mode4RenderLineAll;
    break;
  case 5:
    if((!fxOn && !windowOn && !(layerEnable & 0x8000)) ||
       cpuDisableSfx)
      renderLine = mode5RenderLine;
    else if(fxOn && !windowOn && !(layerEnable & 0x8000))
      renderLine = mode5RenderLineNoWindow;
    else
      renderLine = mode5RenderLineAll;
  default:
    break;
  }
}

void CPUUpdateCPSR()
{
  u32 CPSR = reg[16].I & 0x40;

  CPSR |= (N_FLAG ? 0x80000000 : 0);
  CPSR |= (Z_FLAG ? 0x40000000 : 0);
  CPSR |= (C_FLAG ? 0x20000000 : 0);
  CPSR |= (V_FLAG ? 0x10000000 : 0);
  CPSR |= (armState ? 0 : 0x20);
  CPSR |= (armIrqEnable ? 0 : 0x80);
  CPSR |= (armMode & 0x1F);

  reg[16].I = CPSR;
}

void CPUUpdateFlags(bool breakLoop)
{
  u32 CPSR = reg[16].I;

  // --- WII OPTIMIZATION: BRANCHLESS FLAG EXTRACTION ---
  N_FLAG = (CPSR >> 31) & 1;
  Z_FLAG = (CPSR >> 30) & 1;
  C_FLAG = (CPSR >> 29) & 1;
  V_FLAG = (CPSR >> 28) & 1;
  armState = ((~CPSR) >> 5) & 1;
  armIrqEnable = ((~CPSR) >> 7) & 1;

  // Replaced logical && with pure bitwise logic.
  if(breakLoop) {
      if (armIrqEnable & IME & ((IF & IE) != 0))
        cpuNextEvent = cpuTotalTicks;
  }
}

void CPUUpdateFlags()
{
  CPUUpdateFlags(true);
}

static void CPUSwap(volatile u32 *a, volatile u32 *b)
{
  volatile u32 c = *b;
  *b = *a;
  *a = c;
}

void CPUSwitchMode(int mode, bool saveState, bool breakLoop)
{
  //  if(armMode == mode)
  //    return;

  CPUUpdateCPSR();

  switch(armMode) {
  case 0x10:
  case 0x1F:
    reg[R13_USR].I = reg[13].I;
    reg[R14_USR].I = reg[14].I;
    reg[17].I = reg[16].I;
    break;
  case 0x11:
    CPUSwap(&reg[R8_FIQ].I, &reg[8].I);
    CPUSwap(&reg[R9_FIQ].I, &reg[9].I);
    CPUSwap(&reg[R10_FIQ].I, &reg[10].I);
    CPUSwap(&reg[R11_FIQ].I, &reg[11].I);
    CPUSwap(&reg[R12_FIQ].I, &reg[12].I);
    reg[R13_FIQ].I = reg[13].I;
    reg[R14_FIQ].I = reg[14].I;
    reg[SPSR_FIQ].I = reg[17].I;
    break;
  case 0x12:
    reg[R13_IRQ].I  = reg[13].I;
    reg[R14_IRQ].I  = reg[14].I;
    reg[SPSR_IRQ].I =  reg[17].I;
    break;
  case 0x13:
    reg[R13_SVC].I  = reg[13].I;
    reg[R14_SVC].I  = reg[14].I;
    reg[SPSR_SVC].I =  reg[17].I;
    break;
  case 0x17:
    reg[R13_ABT].I  = reg[13].I;
    reg[R14_ABT].I  = reg[14].I;
    reg[SPSR_ABT].I =  reg[17].I;
    break;
  case 0x1b:
    reg[R13_UND].I  = reg[13].I;
    reg[R14_UND].I  = reg[14].I;
    reg[SPSR_UND].I =  reg[17].I;
    break;
  }

  u32 CPSR = reg[16].I;
  u32 SPSR = reg[17].I;

  switch(mode) {
  case 0x10:
  case 0x1F:
    reg[13].I = reg[R13_USR].I;
    reg[14].I = reg[R14_USR].I;
    reg[16].I = SPSR;
    break;
  case 0x11:
    CPUSwap(&reg[8].I, &reg[R8_FIQ].I);
    CPUSwap(&reg[9].I, &reg[R9_FIQ].I);
    CPUSwap(&reg[10].I, &reg[R10_FIQ].I);
    CPUSwap(&reg[11].I, &reg[R11_FIQ].I);
    CPUSwap(&reg[12].I, &reg[R12_FIQ].I);
    reg[13].I = reg[R13_FIQ].I;
    reg[14].I = reg[R14_FIQ].I;
    if(saveState)
      reg[17].I = CPSR;
    else
      reg[17].I = reg[SPSR_FIQ].I;
    break;
  case 0x12:
    reg[13].I = reg[R13_IRQ].I;
    reg[14].I = reg[R14_IRQ].I;
    reg[16].I = SPSR;
    if(saveState)
      reg[17].I = CPSR;
    else
      reg[17].I = reg[SPSR_IRQ].I;
    break;
  case 0x13:
    reg[13].I = reg[R13_SVC].I;
    reg[14].I = reg[R14_SVC].I;
    reg[16].I = SPSR;
    if(saveState)
      reg[17].I = CPSR;
    else
      reg[17].I = reg[SPSR_SVC].I;
    break;
  case 0x17:
    reg[13].I = reg[R13_ABT].I;
    reg[14].I = reg[R14_ABT].I;
    reg[16].I = SPSR;
    if(saveState)
      reg[17].I = CPSR;
    else
      reg[17].I = reg[SPSR_ABT].I;
    break;
  case 0x1b:
    reg[13].I = reg[R13_UND].I;
    reg[14].I = reg[R14_UND].I;
    reg[16].I = SPSR;
    if(saveState)
      reg[17].I = CPSR;
    else
      reg[17].I = reg[SPSR_UND].I;
    break;
  default:
    systemMessage(MSG_UNSUPPORTED_ARM_MODE, N_("Unsupported ARM mode %02x"), mode);
    break;
  }
  armMode = mode;
  CPUUpdateFlags(breakLoop);
  CPUUpdateCPSR();
}

void CPUSwitchMode(int mode, bool saveState)
{
  CPUSwitchMode(mode, saveState, true);
}

void CPUUndefinedException()
{
  u32 PC = reg[15].I;
  bool savedArmState = armState;
  CPUSwitchMode(0x1b, true, false);
  reg[14].I = PC - (savedArmState ? 4 : 2);
  reg[15].I = 0x04;
  armState = true;
  armIrqEnable = false;
  armNextPC = 0x04;
  ARM_PREFETCH;
  reg[15].I += 4;
}

void CPUSoftwareInterrupt()
{
  u32 PC = reg[15].I;
  bool savedArmState = armState;
  CPUSwitchMode(0x13, true, false);
  reg[14].I = PC - (savedArmState ? 4 : 2);
  reg[15].I = 0x08;
  armState = true;
  armIrqEnable = false;
  armNextPC = 0x08;
  ARM_PREFETCH;
  reg[15].I += 4;
}

// -------------------------------------------------------------------------
// WII OPTIMIZATION: SWI Handlers
// Extracting the massive switch statement into individual handler functions.
// -------------------------------------------------------------------------
typedef void (*SWIHandler)(int);

static void SWI_00_SoftReset(int c) { BIOS_SoftReset(); ARM_PREFETCH; }
static void SWI_01_RegisterRamReset(int c) { BIOS_RegisterRamReset(); }
static void SWI_02_Halt(int c) { holdState = true; holdType = -1; cpuNextEvent = cpuTotalTicks; }
static void SWI_03_Stop(int c) { holdState = true; holdType = -1; stopState = true; cpuNextEvent = cpuTotalTicks; }
static void SWI_04_07_Intr(int c) { CPUSoftwareInterrupt(); }
static void SWI_08_Sqrt(int c) { BIOS_Sqrt(); }
static void SWI_09_ArcTan(int c) { BIOS_ArcTan(); }
static void SWI_0A_ArcTan2(int c) { BIOS_ArcTan2(); }

// -------------------------------------------------------------------------
// WII OPTIMIZATION: Branchless Waitstate Math for SWI 0x0B (CpuSet)
// Replaces nested if/else ternaries with 1-cycle bitwise masks.
// -------------------------------------------------------------------------
static void SWI_0B_CpuSet(int c) {
  u32 len = (reg[2].I & 0x1FFFFF) >> 1;
  u32 r0_hi = reg[0].I & 0xE000000;
  u32 r0_len_hi = (reg[0].I + len) & 0xE000000;

  if (!(r0_hi == 0 || r0_len_hi == 0)) {
    // Branchlessly evaluate bit conditions into 0xFFFFFFFF or 0x00000000 masks
    u32 isFill = (reg[2].I >> 24) & 1;
    u32 is32Bit = (reg[2].I >> 26) & 1;
    u32 maskFill = -isFill;
    u32 mask32   = -is32Bit;

    u32 srcWait = memoryWait[(reg[0].I >> 24) & 0xF];
    u32 srcWait32 = memoryWait32[(reg[0].I >> 24) & 0xF];
    u32 dstWait = memoryWait[(reg[1].I >> 24) & 0xF];
    u32 dstWait32 = memoryWait32[(reg[1].I >> 24) & 0xF];

    // Compute the 32-bit and 16-bit wait permutations on the fly
    u32 wait32 = ((7 + dstWait32) & maskFill) | ((10 + srcWait32 + dstWait32) & ~maskFill);
    u32 wait16 = ((8 + dstWait) & maskFill) | ((11 + srcWait + dstWait) & ~maskFill);

    // Final base tick resolution
    u32 baseWait = (wait32 & mask32) | (wait16 & ~mask32);

    // Branchlessly shift len down if is32Bit is true (>> 1), otherwise it shifts by 0
    u32 finalLen = len >> is32Bit;

    SWITicks = baseWait * finalLen;
  }
  BIOS_CpuSet();
}

// -------------------------------------------------------------------------
// WII OPTIMIZATION: Branchless Waitstate Math for SWI 0x0C (CpuFastSet)
// -------------------------------------------------------------------------
static void SWI_0C_CpuFastSet(int c) {
  u32 len = (reg[2].I & 0x1FFFFF) >> 5;
  u32 r0_hi = reg[0].I & 0xE000000;
  u32 r0_len_hi = (reg[0].I + len) & 0xE000000;

  if (!(r0_hi == 0 || r0_len_hi == 0)) {
    u32 isFill = (reg[2].I >> 24) & 1;
    u32 maskFill = -isFill;

    u32 sm = (reg[0].I >> 24) & 0xF;
    u32 dm = (reg[1].I >> 24) & 0xF;

    u32 fillWait = 6 + memoryWait32[dm] + 7 * (memoryWaitSeq32[dm] + 1);
    u32 copyWait = 9 + memoryWait32[sm] + memoryWait32[dm] + 7 * (memoryWaitSeq32[sm] + memoryWaitSeq32[dm] + 2);

    u32 baseWait = (fillWait & maskFill) | (copyWait & ~maskFill);
    SWITicks = baseWait * len;
  }
  BIOS_CpuFastSet();
}

static void SWI_0D_GetBiosChecksum(int c) { BIOS_GetBiosChecksum(); }
static void SWI_0E_BgAffineSet(int c) { BIOS_BgAffineSet(); }
static void SWI_0F_ObjAffineSet(int c) { BIOS_ObjAffineSet(); }

static void SWI_10_BitUnPack(int c) {
  u32 len = CPUReadHalfWord(reg[2].I);
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + len) & 0xE000000) == 0))
    SWITicks = (32 + memoryWait[(reg[0].I >> 24) & 0xF]) * len;
  BIOS_BitUnPack();
}

static void SWI_11_LZ77UnCompWram(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 8;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (9 + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
  BIOS_LZ77UnCompWram();
}

static void SWI_12_LZ77UnCompVram(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 8;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (19 + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
  BIOS_LZ77UnCompVram();
}

static void SWI_13_HuffUnComp(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 8;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (29 + (memoryWait[(reg[0].I >> 24) & 0xF] << 1)) * len;
  BIOS_HuffUnComp();
}

static void SWI_14_RLUnCompWram(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 8;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (11 + memoryWait[(reg[0].I >> 24) & 0xF] + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
  BIOS_RLUnCompWram();
}

static void SWI_15_RLUnCompVram(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 9;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (34 + (memoryWait[(reg[0].I >> 24) & 0xF] << 1) + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
  BIOS_RLUnCompVram();
}

static void SWI_16_Diff8bitUnFilterWram(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 8;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (13 + memoryWait[(reg[0].I >> 24) & 0xF] + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
  BIOS_Diff8bitUnFilterWram();
}

static void SWI_17_Diff8bitUnFilterVram(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 9;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (39 + (memoryWait[(reg[0].I >> 24) & 0xF] << 1) + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
  BIOS_Diff8bitUnFilterVram();
}

static void SWI_18_Diff16bitUnFilter(int c) {
  u32 len = CPUReadMemory(reg[0].I) >> 9;
  if (!(((reg[0].I & 0xE000000) == 0) || ((reg[0].I + (len & 0x1FFFFF)) & 0xE000000) == 0))
    SWITicks = (13 + memoryWait[(reg[0].I >> 24) & 0xF] + memoryWait[(reg[1].I >> 24) & 0xF]) * len;
  BIOS_Diff16bitUnFilter();
}

static void SWI_19_SoundBiasSet(int c) { if (reg[0].I) soundPause(); else soundResume(); }
static void SWI_1F_MidiKey2Freq(int c) { BIOS_MidiKey2Freq(); }
static void SWI_2A_SndDriverJmpTableCopy(int c) { BIOS_SndDriverJmpTableCopy(); }

static void SWI_Default_Unsupported(int comment) {
  static bool disableMessage = false;

  if (!disableMessage) {
    systemMessage(MSG_UNSUPPORTED_BIOS_FUNCTION,
                  N_("Unsupported BIOS function %02x called from %08x. A BIOS file is needed in order to get correct behaviour."),
                  comment,
                  armMode ? armNextPC - 4 : armNextPC - 2);
    disableMessage = true;
  }
}

// -------------------------------------------------------------------------
// WII OPTIMIZATION: Static Lookup Table (Branchless Dispatch)
// O(1) jump table mapped directly from 0x00 to 0x2A.
// -------------------------------------------------------------------------
static const SWIHandler SWITable[43] = {
  SWI_00_SoftReset, SWI_01_RegisterRamReset, SWI_02_Halt, SWI_03_Stop,
  SWI_04_07_Intr, SWI_04_07_Intr, SWI_04_07_Intr, SWI_04_07_Intr,
  SWI_08_Sqrt, SWI_09_ArcTan, SWI_0A_ArcTan2, SWI_0B_CpuSet,
  SWI_0C_CpuFastSet, SWI_0D_GetBiosChecksum, SWI_0E_BgAffineSet, SWI_0F_ObjAffineSet,
  SWI_10_BitUnPack, SWI_11_LZ77UnCompWram, SWI_12_LZ77UnCompVram, SWI_13_HuffUnComp,
  SWI_14_RLUnCompWram, SWI_15_RLUnCompVram, SWI_16_Diff8bitUnFilterWram, SWI_17_Diff8bitUnFilterVram,
  SWI_18_Diff16bitUnFilter, SWI_19_SoundBiasSet,
  SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported,
  SWI_1F_MidiKey2Freq,
  SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported,
  SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported, SWI_Default_Unsupported,
  SWI_2A_SndDriverJmpTableCopy
};

// -------------------------------------------------------------------------
// WII OPTIMIZATION: Main SWI Dispatcher
// -------------------------------------------------------------------------
void CPUSoftwareInterrupt(int comment)
{
  if (armState) comment >>= 16;

  if (comment == 0xfa) return;

  if (comment == 0xf9) {
    emulating = 0;
    cpuNextEvent = cpuTotalTicks;
    cpuBreakLoop = true;
    return;
  }

  if (useBios) {
    CPUSoftwareInterrupt();
    return;
  }

  // Branchless bounded lookup
  if (comment >= 0 && comment <= 0x2A) {
    SWITable[comment](comment);
  } else {
    SWI_Default_Unsupported(comment);
  }
}

// Use a static inline function instead of a macro for better type safety
// and to allow the compiler to inline the assembly without hidden side effects.
static inline void WriteReg16(u32 address, u16 value) {
    WRITE16LE(((u16 *)&ioMem[address]), value);
}

void CPUCompareVCOUNT()
{
  // --- WII OPTIMIZATION: BRANCHLESS VCOUNT EVALUATION ---
  // Evaluate the match mathematically, leaving the pipeline flowing.
  u32 match = (VCOUNT == (DISPSTAT >> 8));

  // Clear bit 2, then set it dynamically if a match occurred
  DISPSTAT = (DISPSTAT & 0xFFFB) | (match << 2);
  WriteReg16(0x04, DISPSTAT);

  // If match is true AND the VCOUNT IRQ (bit 5) is enabled, trigger IF
  if (match & ((DISPSTAT >> 5) & 1)) {
    IF |= 4;
    WriteReg16(0x202, IF);
  }

  if (layerEnableDelay > 0) {
      --layerEnableDelay;
      if (layerEnableDelay == 1)
          layerEnable = layerSettings & DISPCNT;
  }
}

template <bool transfer32>
static void doDMA_T(u32 &s, u32 &d, u32 si, u32 di, u32 c)
{
  // GBA Hardware Quirk: A transfer count of 0 implies max length.
  if (c == 0) {
      c = transfer32 ? 0x4000 : 0x10000;
  }

  u32 sm = (s >> 24) & 15;
  u32 dm = (d >> 24) & 15;
  u32 sc = c;

  cpuDmaHack = true;
  cpuDmaCount = c;

  // Pin global to a local GPR to prevent continuous stack spilling in the unrolled loop
  u32 localDmaLast = cpuDmaLast;

  if (transfer32) {
    s &= 0xFFFFFFFC;
    if (s < 0x02000000 && (reg[15].I >> 24)) {
      // Bulk transfer unrolling for BIOS masking
      while (c >= 4) {
        CPUWriteMemory(d, 0); d += di;
        CPUWriteMemory(d, 0); d += di;
        CPUWriteMemory(d, 0); d += di;
        CPUWriteMemory(d, 0); d += di;
        c -= 4;
      }
      while (c != 0) {
        CPUWriteMemory(d, 0); d += di;
        --c;
      }
    } else {
      // Bulk 32-bit DMA transfer unrolled for pipelined Load/Store
      while (c >= 4) {
        // Asynchronously prefetch the next unrolled block (16 bytes ahead) securely from the host memory map
        u8 page = (s + (si << 2)) >> 24;
        u8* ptr = gbaReadPagePtrs[page];
        if (ptr) __builtin_prefetch(ptr + ((s + (si << 2)) & gbaReadPageMasks[page]), 0, 0);

        localDmaLast = CPUReadMemory(s); CPUWriteMemory(d, localDmaLast); s += si; d += di;
        localDmaLast = CPUReadMemory(s); CPUWriteMemory(d, localDmaLast); s += si; d += di;
        localDmaLast = CPUReadMemory(s); CPUWriteMemory(d, localDmaLast); s += si; d += di;
        localDmaLast = CPUReadMemory(s); CPUWriteMemory(d, localDmaLast); s += si; d += di;
        c -= 4;
      }
      while (c != 0) {
        localDmaLast = CPUReadMemory(s); CPUWriteMemory(d, localDmaLast); s += si; d += di;
        --c;
      }
    }
  } else {
    s &= 0xFFFFFFFE;
    si = (int)si >> 1;
    di = (int)di >> 1;
    if (s < 0x02000000 && (reg[15].I >> 24)) {
      while (c >= 4) {
        CPUWriteHalfWord(d, 0); d += di;
        CPUWriteHalfWord(d, 0); d += di;
        CPUWriteHalfWord(d, 0); d += di;
        CPUWriteHalfWord(d, 0); d += di;
        c -= 4;
      }
      while (c != 0) {
        CPUWriteHalfWord(d, 0); d += di;
        --c;
      }
    } else {
      // Bulk 16-bit DMA transfer unrolled for pipelined Load/Store
      while (c >= 4) {
        // Asynchronously prefetch the next unrolled block (8 bytes ahead) securely from the host memory map
        u8 page = (s + (si << 2)) >> 24;
        u8* ptr = gbaReadPagePtrs[page];
        if (ptr) __builtin_prefetch(ptr + ((s + (si << 2)) & gbaReadPageMasks[page]), 0, 0);

        localDmaLast = CPUReadHalfWord(s); CPUWriteHalfWord(d, localDmaLast); localDmaLast |= (localDmaLast << 16); s += si; d += di;
        localDmaLast = CPUReadHalfWord(s); CPUWriteHalfWord(d, localDmaLast); localDmaLast |= (localDmaLast << 16); s += si; d += di;
        localDmaLast = CPUReadHalfWord(s); CPUWriteHalfWord(d, localDmaLast); localDmaLast |= (localDmaLast << 16); s += si; d += di;
        localDmaLast = CPUReadHalfWord(s); CPUWriteHalfWord(d, localDmaLast); localDmaLast |= (localDmaLast << 16); s += si; d += di;
        c -= 4;
      }
      while (c != 0) {
        localDmaLast = CPUReadHalfWord(s); CPUWriteHalfWord(d, localDmaLast); localDmaLast |= (localDmaLast << 16); s += si; d += di;
        --c;
      }
    }
  }

  // Restore the open bus state globally once the pipeline has finished
  cpuDmaLast = localDmaLast;
  cpuDmaCount = 0;

  // Since transfer32 is a template argument, the compiler eliminates these ternaries entirely at compile time.
  u32 sw = 1 + (transfer32 ? memoryWaitSeq32[sm] : memoryWaitSeq[sm]);
  u32 dw = 1 + (transfer32 ? memoryWaitSeq32[dm] : memoryWaitSeq[dm]);
  u32 waitBase = 6 + (transfer32 ? memoryWait32[sm] + memoryWaitSeq32[dm] : memoryWait[sm] + memoryWaitSeq[dm]);

  u32 tickAdd = sw + dw;
  u32 t = sc - 1;
  u32 totalWait = waitBase + (tickAdd * t);

  cpuDmaTicksToUpdate += totalWait;
  cpuDmaHack = false;
}

// Runtime dispatcher
void doDMA(u32 &s, u32 &d, u32 si, u32 di, u32 c, int transfer32)
{
  if (transfer32) {
    doDMA_T<true>(s, d, si, di, c);
  } else {
    doDMA_T<false>(s, d, si, di, c);
  }
}

// -------------------------------------------------------------------------
// WII OPTIMIZATION: Template unrolling for CPUCheckDMA
// Replaces repetitive DMA logic with a single unified template.
// Uses C++ implicit boolean casts for branchless increment and count selection.
// -------------------------------------------------------------------------
template <int channel>
static inline void CPUCheckDMA_T(int reason, int dmamask, u16& cnt_h, u16 cnt_l, u32& src, u32& dst, u16 dad_h, u16 dad_l) {

  // 1-Cycle Bitwise Validation Mask: Bypasses sequential short-circuit branching overhead.
  // 1. Is DMA enabled? (cnt_h & 0x8000)
  // 2. Is channel triggered in mask? -> shifted to 0x8000
  // 3. Does reason match? -> shifted to 0x8000
  u32 valid = (cnt_h & 0x8000) & (((dmamask >> channel) & 1) << 15) & ((((cnt_h >> 12) & 3) == reason) << 15);

  // Fast-fail natively with a single predictable branch
  if (!valid) return;

  u32 ctrlSrc = (cnt_h >> 7) & 3;
  u32 ctrlDst = (cnt_h >> 5) & 3;

  // Branchless source increment (0: 4, 1: -4, 2: 0, 3: 4)
  // Maps to Broadway's 1-cycle bitwise and arithmetic units without branching.
  u32 src_is_dec  = (ctrlSrc == 1);
  u32 src_is_zero = (ctrlSrc == 2);
  u32 sourceIncrement = (4 - (src_is_dec << 3)) & (src_is_zero - 1);

  // Branchless destination increment
  u32 dst_is_dec  = (ctrlDst == 1);
  u32 dst_is_zero = (ctrlDst == 2);
  u32 destIncrement = (4 - (dst_is_dec << 3)) & (dst_is_zero - 1);

  u32 count;
  u32 transfer32 = cnt_h & 0x0400;

  // DMA 1 and 2 specific logic (Audio FIFO overrides)
  if (channel == 1 || channel == 2) {
    if (reason == 3) {
      count = 4;
      destIncrement = 0;
      transfer32 = 0x0400;
    } else {
      u32 is_zero = (cnt_l == 0);
      count = cnt_l | (is_zero << 14); // Branchlessly evaluates: cnt_l ? cnt_l : 0x4000
    }
  } else if (channel == 0) {
    u32 is_zero = (cnt_l == 0);
    count = cnt_l | (is_zero << 14);
  } else {
    // channel == 3
    u32 is_zero = (cnt_l == 0);
    count = cnt_l | (is_zero << 16); // Branchlessly evaluates: cnt_l ? cnt_l : 0x10000
  }

  // Execute the DMA transfer
  doDMA(src, dst, sourceIncrement, destIncrement, count, transfer32);

  // IRQ Generation
  if (cnt_h & 0x4000) {
    IF |= (0x0100 << channel);
    WriteReg16(0x202, IF);
    cpuNextEvent = cpuTotalTicks;
  }

  // Reload Destination (Mode 3)
  if (ctrlDst == 3) {
    dst = dad_l | (dad_h << 16);
  }

  // Define the register addresses in an array to eliminate branching
  static const u16 DMA_CNT_H_REGS[] = { 0xBA, 0xC6, 0xD2, 0xDE };

  // Auto-disable if not repeating
  if (!(cnt_h & 0x0200) || (reason == 0)) {
      cnt_h &= 0x7FFF;
      WriteReg16(DMA_CNT_H_REGS[channel], cnt_h);
  }
}

// -------------------------------------------------------------------------
// WII OPTIMIZATION: Main DMA Dispatcher
// Compiles down to 4 highly-inlined, specific executions.
// -------------------------------------------------------------------------
void CPUCheckDMA(int reason, int dmamask) {
  CPUCheckDMA_T<0>(reason, dmamask, DM0CNT_H, DM0CNT_L, dma0Source, dma0Dest, DM0DAD_H, DM0DAD_L);
  CPUCheckDMA_T<1>(reason, dmamask, DM1CNT_H, DM1CNT_L, dma1Source, dma1Dest, DM1DAD_H, DM1DAD_L);
  CPUCheckDMA_T<2>(reason, dmamask, DM2CNT_H, DM2CNT_L, dma2Source, dma2Dest, DM2DAD_H, DM2DAD_L);
  CPUCheckDMA_T<3>(reason, dmamask, DM3CNT_H, DM3CNT_L, dma3Source, dma3Dest, DM3DAD_H, DM3DAD_L);
}

void CPUUpdateRegister(u32 address, u16 value)
{
  // --- WII OPTIMIZATION: FAST-PATH DISPATCHER ---
  // Bypassing the massive switch statement prevents I-Cache eviction
  // and branch predictor thrashing for the 90% most common IO writes.

  if (address == 0x202) { // IF - Interrupt Flag
    IF ^= (value & IF);
    WriteReg16(0x202, IF);
    return;
  }
  if (address == 0x208) { // IME - Interrupt Master Enable
    IME = value & 1;
    WriteReg16(0x208, IME);
    // Replaced short-circuit && with bitwise &
    if ((IME & 1) & armIrqEnable & ((IF & IE) != 0))
      cpuNextEvent = cpuTotalTicks;
    return;
  }
  if (address == 0x200) { // IE - Interrupt Enable
    IE = value & 0x3FFF;
    WriteReg16(0x200, IE);
    if ((IME & 1) & armIrqEnable & ((IF & IE) != 0))
      cpuNextEvent = cpuTotalTicks;
    return;
  }
  if (address == 0x04) { // DISPSTAT - Display Status
    DISPSTAT = (value & 0xFF38) | (DISPSTAT & 7);
    WriteReg16(0x04, DISPSTAT);
    return;
  }

  // --- FALLBACK JUMP TABLE ---
  switch(address)
  {
  case 0x00:
    { // we need to place the following code in { } because we declare & initialize variables in a case statement
      if((value & 7) > 5) {
        // display modes above 0-5 are prohibited
        DISPCNT = (value & 7);
      }
      bool change = (0 != ((DISPCNT ^ value) & 0x80));
      bool changeBG = (0 != ((DISPCNT ^ value) & 0x0F00));
      u16 changeBGon = ((~DISPCNT) & value) & 0x0F00; // these layers are being activated

      DISPCNT = (value & 0xFFF7); // bit 3 can only be accessed by the BIOS to enable GBC mode
      WriteReg16(0x00, DISPCNT);

      if(changeBGon) {
        layerEnableDelay = 4;
        layerEnable = layerSettings & value & (~changeBGon);
      } else {
        layerEnable = layerSettings & value;
        // CPUUpdateTicks();
      }

      windowOn = (layerEnable & 0x6000) ? true : false;
      if(change && !((value & 0x80))) {
        if(!(DISPSTAT & 1)) {
          lcdTicks = 1008;
          DISPSTAT &= 0xFFFC;
          WriteReg16(0x04, DISPSTAT);
          CPUCompareVCOUNT();
        }
      }
      CPUUpdateRender();
      // we only care about changes in BG0-BG3
      if(changeBG) {
        CPUUpdateRenderBuffers(false);
      }
      break;
    }
  case 0x06:
    // not writable
    break;
  case 0x08:
    BG0CNT = (value & 0xDFCF);
    WriteReg16(0x08, BG0CNT);
    break;
  case 0x0A:
    BG1CNT = (value & 0xDFCF);
    WriteReg16(0x0A, BG1CNT);
    break;
  case 0x0C:
    BG2CNT = (value & 0xFFCF);
    WriteReg16(0x0C, BG2CNT);
    break;
  case 0x0E:
    BG3CNT = (value & 0xFFCF);
    WriteReg16(0x0E, BG3CNT);
    break;
  case 0x10:
    BG0HOFS = value & 511;
    WriteReg16(0x10, BG0HOFS);
    break;
  case 0x12:
    BG0VOFS = value & 511;
    WriteReg16(0x12, BG0VOFS);
    break;
  case 0x14:
    BG1HOFS = value & 511;
    WriteReg16(0x14, BG1HOFS);
    break;
  case 0x16:
    BG1VOFS = value & 511;
    WriteReg16(0x16, BG1VOFS);
    break;
  case 0x18:
    BG2HOFS = value & 511;
    WriteReg16(0x18, BG2HOFS);
    break;
  case 0x1A:
    BG2VOFS = value & 511;
    WriteReg16(0x1A, BG2VOFS);
    break;
  case 0x1C:
    BG3HOFS = value & 511;
    WriteReg16(0x1C, BG3HOFS);
    break;
  case 0x1E:
    BG3VOFS = value & 511;
    WriteReg16(0x1E, BG3VOFS);
    break;
  case 0x20:
    BG2PA = value;
    WriteReg16(0x20, BG2PA);
    break;
  case 0x22:
    BG2PB = value;
    WriteReg16(0x22, BG2PB);
    break;
  case 0x24:
    BG2PC = value;
    WriteReg16(0x24, BG2PC);
    break;
  case 0x26:
    BG2PD = value;
    WriteReg16(0x26, BG2PD);
    break;
  case 0x28:
    BG2X_L = value;
    WriteReg16(0x28, BG2X_L);
    gfxBG2Changed |= 1;
    break;
  case 0x2A:
    BG2X_H = (value & 0xFFF);
    WriteReg16(0x2A, BG2X_H);
    gfxBG2Changed |= 1;
    break;
  case 0x2C:
    BG2Y_L = value;
    WriteReg16(0x2C, BG2Y_L);
    gfxBG2Changed |= 2;
    break;
  case 0x2E:
    BG2Y_H = value & 0xFFF;
    WriteReg16(0x2E, BG2Y_H);
    gfxBG2Changed |= 2;
    break;
  case 0x30:
    BG3PA = value;
    WriteReg16(0x30, BG3PA);
    break;
  case 0x32:
    BG3PB = value;
    WriteReg16(0x32, BG3PB);
    break;
  case 0x34:
    BG3PC = value;
    WriteReg16(0x34, BG3PC);
    break;
  case 0x36:
    BG3PD = value;
    WriteReg16(0x36, BG3PD);
    break;
  case 0x38:
    BG3X_L = value;
    WriteReg16(0x38, BG3X_L);
    gfxBG3Changed |= 1;
    break;
  case 0x3A:
    BG3X_H = value & 0xFFF;
    WriteReg16(0x3A, BG3X_H);
    gfxBG3Changed |= 1;
    break;
  case 0x3C:
    BG3Y_L = value;
    WriteReg16(0x3C, BG3Y_L);
    gfxBG3Changed |= 2;
    break;
  case 0x3E:
    BG3Y_H = value & 0xFFF;
    WriteReg16(0x3E, BG3Y_H);
    gfxBG3Changed |= 2;
    break;
  case 0x40:
    WIN0H = value;
    WriteReg16(0x40, WIN0H);
    CPUUpdateWindow0();
    break;
  case 0x42:
    WIN1H = value;
    WriteReg16(0x42, WIN1H);
    CPUUpdateWindow1();
    break;
  case 0x44:
    WIN0V = value;
    WriteReg16(0x44, WIN0V);
    break;
  case 0x46:
    WIN1V = value;
    WriteReg16(0x46, WIN1V);
    break;
  case 0x48:
    WININ = value & 0x3F3F;
    WriteReg16(0x48, WININ);
    break;
  case 0x4A:
    WINOUT = value & 0x3F3F;
    WriteReg16(0x4A, WINOUT);
    break;
  case 0x4C:
    MOSAIC = value;
    WriteReg16(0x4C, MOSAIC);
    break;
  case 0x50:
    BLDMOD = value & 0x3FFF;
    WriteReg16(0x50, BLDMOD);
    fxOn = ((BLDMOD>>6)&3) != 0;
    CPUUpdateRender();
    break;
  case 0x52:
    COLEV = value & 0x1F1F;
    WriteReg16(0x52, COLEV);
    break;
  case 0x54:
    COLY = value & 0x1F;
    WriteReg16(0x54, COLY);
    break;
  case 0x60:
  case 0x62:
  case 0x64:
  case 0x68:
  case 0x6c:
  case 0x70:
  case 0x72:
  case 0x74:
  case 0x78:
  case 0x7c:
  case 0x80:
  case 0x84:
    soundEvent(address&0xFF, (u8)(value & 0xFF));
    soundEvent((address&0xFF)+1, (u8)(value>>8));
    break;
  case 0x82:
  case 0x88:
  case 0xa0:
  case 0xa2:
  case 0xa4:
  case 0xa6:
  case 0x90:
  case 0x92:
  case 0x94:
  case 0x96:
  case 0x98:
  case 0x9a:
  case 0x9c:
  case 0x9e:
    soundEvent(address&0xFF, value);
    break;
  case 0xB0:
    DM0SAD_L = value;
    WriteReg16(0xB0, DM0SAD_L);
    break;
  case 0xB2:
    DM0SAD_H = value & 0x07FF;
    WriteReg16(0xB2, DM0SAD_H);
    break;
  case 0xB4:
    DM0DAD_L = value;
    WriteReg16(0xB4, DM0DAD_L);
    break;
  case 0xB6:
    DM0DAD_H = value & 0x07FF;
    WriteReg16(0xB6, DM0DAD_H);
    break;
  case 0xB8:
    DM0CNT_L = value & 0x3FFF;
    WriteReg16(0xB8, 0);
    break;
  case 0xBA:
    {
      bool start = ((DM0CNT_H ^ value) & 0x8000) ? true : false;
      value &= 0xF7E0;

      DM0CNT_H = value;
      WriteReg16(0xBA, DM0CNT_H);

      if(start && (value & 0x8000)) {
        dma0Source = DM0SAD_L | (DM0SAD_H << 16);
        dma0Dest = DM0DAD_L | (DM0DAD_H << 16);
        CPUCheckDMA(0, 1);
      }
    }
    break;
  case 0xBC:
    DM1SAD_L = value;
    WriteReg16(0xBC, DM1SAD_L);
    break;
  case 0xBE:
    DM1SAD_H = value & 0x0FFF;
    WriteReg16(0xBE, DM1SAD_H);
    break;
  case 0xC0:
    DM1DAD_L = value;
    WriteReg16(0xC0, DM1DAD_L);
    break;
  case 0xC2:
    DM1DAD_H = value & 0x07FF;
    WriteReg16(0xC2, DM1DAD_H);
    break;
  case 0xC4:
    DM1CNT_L = value & 0x3FFF;
    WriteReg16(0xC4, 0);
    break;
  case 0xC6:
    {
      bool start = ((DM1CNT_H ^ value) & 0x8000) ? true : false;
      value &= 0xF7E0;

      DM1CNT_H = value;
      WriteReg16(0xC6, DM1CNT_H);

      if(start && (value & 0x8000)) {
        dma1Source = DM1SAD_L | (DM1SAD_H << 16);
        dma1Dest = DM1DAD_L | (DM1DAD_H << 16);
        CPUCheckDMA(0, 2);
      }
    }
    break;
  case 0xC8:
    DM2SAD_L = value;
    WriteReg16(0xC8, DM2SAD_L);
    break;
  case 0xCA:
    DM2SAD_H = value & 0x0FFF;
    WriteReg16(0xCA, DM2SAD_H);
    break;
  case 0xCC:
    DM2DAD_L = value;
    WriteReg16(0xCC, DM2DAD_L);
    break;
  case 0xCE:
    DM2DAD_H = value & 0x07FF;
    WriteReg16(0xCE, DM2DAD_H);
    break;
  case 0xD0:
    DM2CNT_L = value & 0x3FFF;
    WriteReg16(0xD0, 0);
    break;
  case 0xD2:
    {
      bool start = ((DM2CNT_H ^ value) & 0x8000) ? true : false;

      value &= 0xF7E0;

      DM2CNT_H = value;
      WriteReg16(0xD2, DM2CNT_H);

      if(start && (value & 0x8000)) {
        dma2Source = DM2SAD_L | (DM2SAD_H << 16);
        dma2Dest = DM2DAD_L | (DM2DAD_H << 16);

        CPUCheckDMA(0, 4);
      }
    }
    break;
  case 0xD4:
    DM3SAD_L = value;
    WriteReg16(0xD4, DM3SAD_L);
    break;
  case 0xD6:
    DM3SAD_H = value & 0x0FFF;
    WriteReg16(0xD6, DM3SAD_H);
    break;
  case 0xD8:
    DM3DAD_L = value;
    WriteReg16(0xD8, DM3DAD_L);
    break;
  case 0xDA:
    DM3DAD_H = value & 0x0FFF;
    WriteReg16(0xDA, DM3DAD_H);
    break;
  case 0xDC:
    DM3CNT_L = value;
    WriteReg16(0xDC, 0);
    break;
  case 0xDE:
    {
      bool start = ((DM3CNT_H ^ value) & 0x8000) ? true : false;

      value &= 0xFFE0;

      DM3CNT_H = value;
      WriteReg16(0xDE, DM3CNT_H);

      if(start && (value & 0x8000)) {
        dma3Source = DM3SAD_L | (DM3SAD_H << 16);
        dma3Dest = DM3DAD_L | (DM3DAD_H << 16);
        CPUCheckDMA(0,8);
      }
    }
    break;
  case 0x100:
    timer0Reload = value;
    interp_rate();
    break;
  case 0x102:
    timer0Value = value;
    timerOnOffDelay|=1;
    cpuNextEvent = cpuTotalTicks;
    break;
  case 0x104:
    timer1Reload = value;
    interp_rate();
    break;
  case 0x106:
    timer1Value = value;
    timerOnOffDelay|=2;
    cpuNextEvent = cpuTotalTicks;
    break;
  case 0x108:
    timer2Reload = value;
    break;
  case 0x10A:
    timer2Value = value;
    timerOnOffDelay|=4;
    cpuNextEvent = cpuTotalTicks;
    break;
  case 0x10C:
    timer3Reload = value;
    break;
  case 0x10E:
    timer3Value = value;
    timerOnOffDelay|=8;
    cpuNextEvent = cpuTotalTicks;
    break;
  case 0x128:
      if(value & 0x80) {
        value &= 0xff7f;
        if(value & 1 && (value & 0x4000)) {
          WriteReg16(0x12a, 0xFF);
          IF |= 0x80;
          WriteReg16(0x202, IF);
          value &= 0x7f7f;
        }
      }
      WriteReg16(0x128, value);
    break;
  case 0x12a:
      WriteReg16(0x134, value);
    break;
  case 0x130:
    P1 |= (value & 0x3FF);
    WriteReg16(0x130, P1);
    break;
  case 0x132:
    WriteReg16(0x132, value & 0xC3FF);
    break;
  case 0x134:
      WriteReg16(0x134, value);
    break;
  case 0x140:
      WriteReg16(0x140, value);
    break;
  case 0x204:
    {
      memoryWait[0x0e] = memoryWaitSeq[0x0e] = gamepakRamWaitState[value & 3];

      if(!speedHack) {
        memoryWait[0x08] = memoryWait[0x09] = gamepakWaitState[(value >> 2) & 3];
        memoryWaitSeq[0x08] = memoryWaitSeq[0x09] =
          gamepakWaitState0[(value >> 4) & 1];

        memoryWait[0x0a] = memoryWait[0x0b] = gamepakWaitState[(value >> 5) & 3];
        memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] =
          gamepakWaitState1[(value >> 7) & 1];

        memoryWait[0x0c] = memoryWait[0x0d] = gamepakWaitState[(value >> 8) & 3];
        memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] =
          gamepakWaitState2[(value >> 10) & 1];
      } else {
        memoryWait[0x08] = memoryWait[0x09] = 3;
        memoryWaitSeq[0x08] = memoryWaitSeq[0x09] = 1;

        memoryWait[0x0a] = memoryWait[0x0b] = 3;
        memoryWaitSeq[0x0a] = memoryWaitSeq[0x0b] = 1;

        memoryWait[0x0c] = memoryWait[0x0d] = 3;
        memoryWaitSeq[0x0c] = memoryWaitSeq[0x0d] = 1;
      }

      for(int i = 8; i < 15; i++) {
        memoryWait32[i] = memoryWait[i] + memoryWaitSeq[i] + 1;
        memoryWaitSeq32[i] = (memoryWaitSeq[i]<<1) + 1;
      }

      if((value & 0x4000) == 0x4000) {
        busPrefetchEnable = true;
        busPrefetch = false;
        busPrefetchCount = 0;
      } else {
        busPrefetchEnable = false;
        busPrefetch = false;
        busPrefetchCount = 0;
      }
      WriteReg16(0x204, value & 0x7FFF);

    }
    break;
  case 0x300:
    if(value != 0)
      value &= 0xFFFE;
    WriteReg16(0x300, value);
    break;
  default:
    WriteReg16(address&0x3FE, value);
    break;
  }
}

void applyTimer ()
{
  if (timerOnOffDelay & 1)
  {
    timer0ClockReload = TIMER_TICKS[timer0Value & 3];
    if(!timer0On && (timer0Value & 0x80)) {
      // reload the counter
      TM0D = timer0Reload;
      timer0Ticks = (0x10000 - TM0D) << timer0ClockReload;
      WriteReg16(0x100, TM0D);
    }
    timer0On = timer0Value & 0x80 ? true : false;
    TM0CNT = timer0Value & 0xC7;
    interp_rate();
    WriteReg16(0x102, TM0CNT);
    //    CPUUpdateTicks();
  }
  if (timerOnOffDelay & 2)
  {
    timer1ClockReload = TIMER_TICKS[timer1Value & 3];
    if(!timer1On && (timer1Value & 0x80)) {
      // reload the counter
      TM1D = timer1Reload;
      timer1Ticks = (0x10000 - TM1D) << timer1ClockReload;
      WriteReg16(0x104, TM1D);
    }
    timer1On = timer1Value & 0x80 ? true : false;
    TM1CNT = timer1Value & 0xC7;
    interp_rate();
    WriteReg16(0x106, TM1CNT);
  }
  if (timerOnOffDelay & 4)
  {
    timer2ClockReload = TIMER_TICKS[timer2Value & 3];
    if(!timer2On && (timer2Value & 0x80)) {
      // reload the counter
      TM2D = timer2Reload;
      timer2Ticks = (0x10000 - TM2D) << timer2ClockReload;
      WriteReg16(0x108, TM2D);
    }
    timer2On = timer2Value & 0x80 ? true : false;
    TM2CNT = timer2Value & 0xC7;
    WriteReg16(0x10A, TM2CNT);
  }
  if (timerOnOffDelay & 8)
  {
    timer3ClockReload = TIMER_TICKS[timer3Value & 3];
    if(!timer3On && (timer3Value & 0x80)) {
      // reload the counter
      TM3D = timer3Reload;
      timer3Ticks = (0x10000 - TM3D) << timer3ClockReload;
      WriteReg16(0x10C, TM3D);
    }
    timer3On = timer3Value & 0x80 ? true : false;
    TM3CNT = timer3Value & 0xC7;
    WriteReg16(0x10E, TM3CNT);
  }
  cpuNextEvent = CPUUpdateTicks();
  timerOnOffDelay = 0;
}

u8 cpuBitsSet[256];
u8 cpuLowestBitSet[256];

u8* gbaReadPagePtrs[256];
u32 gbaReadPageMasks[256];
u8* gbaWritePagePtrs[256];

static inline void GBA_InitMemoryPages() {
    for (int i = 0; i < 256; i++) {
        gbaReadPagePtrs[i]  = NULL;
        gbaReadPageMasks[i] = 0;
        gbaWritePagePtrs[i] = NULL;
    }

    // 0x02: WRAM
    gbaReadPagePtrs[0x02]  = workRAM;
    gbaReadPageMasks[0x02] = 0x3FFFF;
    gbaWritePagePtrs[0x02] = workRAM;

    // 0x03: IRAM
    gbaReadPagePtrs[0x03]  = internalRAM;
    gbaReadPageMasks[0x03] = 0x7FFF;
    gbaWritePagePtrs[0x03] = internalRAM;

    // 0x05: PaletteRAM
    // Write array is intentionally left NULL to force Corvette hack checks via slow-path.
    gbaReadPagePtrs[0x05]  = paletteRAM;
    gbaReadPageMasks[0x05] = 0x3FF;

    // 0x07: OAM
    gbaReadPagePtrs[0x07]  = oam;
    gbaReadPageMasks[0x07] = 0x3FF;
    gbaWritePagePtrs[0x07] = oam;

    // 0x09 - 0x0C: ROM (Wait States 1 & 2)
    // 0x08 is intentionally left NULL to force RTC checks via slow-path.
    // Only populate physical ROM pointers if GC Virtual Memory is disabled
#ifndef USE_VM
    for (int i = 0x09; i <= 0x0C; i++) {
        gbaReadPagePtrs[i]  = rom;
        gbaReadPageMasks[i] = 0x1FFFFFF;
    }
#endif
}

void CPUInit(const char *biosFileName, bool useBiosFile)
{
  if(!cpuBiosSwapped) {
    for(unsigned int i = 0; i < sizeof(myROM)/4; i++) {
      WRITE32LE(&myROM[i], myROM[i]);
    }
    cpuBiosSwapped = true;
  }

  gbaSaveType = 0;
  eepromInUse = 0;
  saveType = 0;
  memcpy(bios, myROM, sizeof(myROM));

  GBA_InitMemoryPages();

  int i = 0;

  biosProtected[0] = 0x00;
  biosProtected[1] = 0xf0;
  biosProtected[2] = 0x29;
  biosProtected[3] = 0xe1;

  for(i = 0; i < 256; i++) {
    int count = 0;
    int j;
    for(j = 0; j < 8; j++)
      if(i & (1 << j))
        count++;
    cpuBitsSet[i] = count;

    for(j = 0; j < 8; j++)
      if(i & (1 << j))
        break;
    cpuLowestBitSet[i] = j;
  }

  for(i = 0; i < 0x400; i++)
    ioReadable[i] = true;
  for(i = 0x10; i < 0x48; i++)
    ioReadable[i] = false;
  for(i = 0x4c; i < 0x50; i++)
    ioReadable[i] = false;
  for(i = 0x54; i < 0x60; i++)
    ioReadable[i] = false;
  for(i = 0x8c; i < 0x90; i++)
    ioReadable[i] = false;
  for(i = 0xa0; i < 0xb8; i++)
    ioReadable[i] = false;
  for(i = 0xbc; i < 0xc4; i++)
    ioReadable[i] = false;
  for(i = 0xc8; i < 0xd0; i++)
    ioReadable[i] = false;
  for(i = 0xd4; i < 0xdc; i++)
    ioReadable[i] = false;
  for(i = 0xe0; i < 0x100; i++)
    ioReadable[i] = false;
  for(i = 0x110; i < 0x120; i++)
    ioReadable[i] = false;
  for(i = 0x12c; i < 0x130; i++)
    ioReadable[i] = false;
  for(i = 0x138; i < 0x140; i++)
    ioReadable[i] = false;
  for(i = 0x144; i < 0x150; i++)
    ioReadable[i] = false;
  for(i = 0x15c; i < 0x200; i++)
    ioReadable[i] = false;
  for(i = 0x20c; i < 0x300; i++)
    ioReadable[i] = false;
  for(i = 0x304; i < 0x400; i++)
    ioReadable[i] = false;

  if(romSize < 0x1fe2000) {
    *((u16 *)&rom[0x1fe209c]) = 0xdffa; // SWI 0xFA
    *((u16 *)&rom[0x1fe209e]) = 0x4770; // BX LR
  }
}

void CPUReset()
{
  systemCartridgeRumble(false);
  if(gbaSaveType == 0) {
    if(eepromInUse)
      gbaSaveType = 3;
    else
      switch(saveType) {
      case 1:
        gbaSaveType = 1;
        break;
      case 2:
        gbaSaveType = 2;
        break;
      }
  }
  rtcReset();
  // clean registers
  memset(&reg[0], 0, sizeof(reg));
  // clean OAM
  memset(oam, 0, 0x400);
  // clean palette
  memset(paletteRAM, 0, 0x400);
  // clean picture
  memset(pix, 0, 4*160*240);
  // clean vram
  memset(vram, 0, 0x20000);
  // clean io memory
  memset(ioMem, 0, 0x400);

  DISPCNT  = 0x0080;
  DISPSTAT = 0x0000;
  VCOUNT   = (useBios && !skipBios) ? 0 :0x007E;
  BG0CNT   = 0x0000;
  BG1CNT   = 0x0000;
  BG2CNT   = 0x0000;
  BG3CNT   = 0x0000;
  BG0HOFS  = 0x0000;
  BG0VOFS  = 0x0000;
  BG1HOFS  = 0x0000;
  BG1VOFS  = 0x0000;
  BG2HOFS  = 0x0000;
  BG2VOFS  = 0x0000;
  BG3HOFS  = 0x0000;
  BG3VOFS  = 0x0000;
  BG2PA    = 0x0100;
  BG2PB    = 0x0000;
  BG2PC    = 0x0000;
  BG2PD    = 0x0100;
  BG2X_L   = 0x0000;
  BG2X_H   = 0x0000;
  BG2Y_L   = 0x0000;
  BG2Y_H   = 0x0000;
  BG3PA    = 0x0100;
  BG3PB    = 0x0000;
  BG3PC    = 0x0000;
  BG3PD    = 0x0100;
  BG3X_L   = 0x0000;
  BG3X_H   = 0x0000;
  BG3Y_L   = 0x0000;
  BG3Y_H   = 0x0000;
  WIN0H    = 0x0000;
  WIN1H    = 0x0000;
  WIN0V    = 0x0000;
  WIN1V    = 0x0000;
  WININ    = 0x0000;
  WINOUT   = 0x0000;
  MOSAIC   = 0x0000;
  BLDMOD   = 0x0000;
  COLEV    = 0x0000;
  COLY     = 0x0000;
  DM0SAD_L = 0x0000;
  DM0SAD_H = 0x0000;
  DM0DAD_L = 0x0000;
  DM0DAD_H = 0x0000;
  DM0CNT_L = 0x0000;
  DM0CNT_H = 0x0000;
  DM1SAD_L = 0x0000;
  DM1SAD_H = 0x0000;
  DM1DAD_L = 0x0000;
  DM1DAD_H = 0x0000;
  DM1CNT_L = 0x0000;
  DM1CNT_H = 0x0000;
  DM2SAD_L = 0x0000;
  DM2SAD_H = 0x0000;
  DM2DAD_L = 0x0000;
  DM2DAD_H = 0x0000;
  DM2CNT_L = 0x0000;
  DM2CNT_H = 0x0000;
  DM3SAD_L = 0x0000;
  DM3SAD_H = 0x0000;
  DM3DAD_L = 0x0000;
  DM3DAD_H = 0x0000;
  DM3CNT_L = 0x0000;
  DM3CNT_H = 0x0000;
  TM0D     = 0x0000;
  TM0CNT   = 0x0000;
  TM1D     = 0x0000;
  TM1CNT   = 0x0000;
  TM2D     = 0x0000;
  TM2CNT   = 0x0000;
  TM3D     = 0x0000;
  TM3CNT   = 0x0000;
  P1       = 0x03FF;
  IE       = 0x0000;
  IF       = 0x0000;
  IME      = 0x0000;

  armMode = 0x1F;

  if(cpuIsMultiBoot) {
    reg[13].I = 0x03007F00;
    reg[15].I = 0x02000000;
    reg[16].I = 0x00000000;
    reg[R13_IRQ].I = 0x03007FA0;
    reg[R13_SVC].I = 0x03007FE0;
    armIrqEnable = true;
  } else {
    if(useBios && !skipBios) {
      reg[15].I = 0x00000000;
      armMode = 0x13;
      armIrqEnable = false;
    } else {
      reg[13].I = 0x03007F00;
      reg[15].I = 0x08000000;
      reg[16].I = 0x00000000;
      reg[R13_IRQ].I = 0x03007FA0;
      reg[R13_SVC].I = 0x03007FE0;
      armIrqEnable = true;
    }
  }
  armState = true;
  C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;
  WriteReg16(0x00, DISPCNT);
  WriteReg16(0x06, VCOUNT);
  WriteReg16(0x20, BG2PA);
  WriteReg16(0x26, BG2PD);
  WriteReg16(0x30, BG3PA);
  WriteReg16(0x36, BG3PD);
  WriteReg16(0x130, P1);
  WriteReg16(0x88, 0x200);

  // disable FIQ
  reg[16].I |= 0x40;

  CPUUpdateCPSR();

  armNextPC = reg[15].I;
  reg[15].I += 4;

  // reset internal state
  holdState = false;
  holdType = 0;

  biosProtected[0] = 0x00;
  biosProtected[1] = 0xf0;
  biosProtected[2] = 0x29;
  biosProtected[3] = 0xe1;

  lcdTicks = (useBios && !skipBios) ? 1008 : 208;
  timer0On = false;
  timer0Ticks = 0;
  timer0Reload = 0;
  timer0ClockReload  = 0;
  timer1On = false;
  timer1Ticks = 0;
  timer1Reload = 0;
  timer1ClockReload  = 0;
  timer2On = false;
  timer2Ticks = 0;
  timer2Reload = 0;
  timer2ClockReload  = 0;
  timer3On = false;
  timer3Ticks = 0;
  timer3Reload = 0;
  timer3ClockReload  = 0;
  dma0Source = 0;
  dma0Dest = 0;
  dma1Source = 0;
  dma1Dest = 0;
  dma2Source = 0;
  dma2Dest = 0;
  dma3Source = 0;
  dma3Dest = 0;
  cpuSaveGameFunc = flashSaveDecide;
  renderLine = mode0RenderLine;
  fxOn = false;
  windowOn = false;
  frameCount = 0;
  saveType = 0;
  layerEnable = DISPCNT & layerSettings;

  CPUUpdateRenderBuffers(true);

  for(int i = 0; i < 256; i++) {
    map[i].address = (u8 *)&dummyAddress;
    map[i].mask = 0;
  }

  map[0].address = bios;
  map[0].mask = 0x3FFF;
  map[2].address = workRAM;
  map[2].mask = 0x3FFFF;
  map[3].address = internalRAM;
  map[3].mask = 0x7FFF;
  map[4].address = ioMem;
  map[4].mask = 0x3FF;
  map[5].address = paletteRAM;
  map[5].mask = 0x3FF;
  map[6].address = vram;
  map[6].mask = 0x1FFFF;
  map[7].address = oam;
  map[7].mask = 0x3FF;
  map[8].address = rom;
  map[8].mask = 0x1FFFFFF;
  map[9].address = rom;
  map[9].mask = 0x1FFFFFF;
  map[10].address = rom;
  map[10].mask = 0x1FFFFFF;
  map[12].address = rom;
  map[12].mask = 0x1FFFFFF;
  map[14].address = flashSaveMemory;
  map[14].mask = 0xFFFF;

  eepromReset();
  flashReset();

  soundReset();

  CPUUpdateWindow0();
  CPUUpdateWindow1();

  // make sure registers are correctly initialized if not using BIOS
  if(!useBios) {
    if(cpuIsMultiBoot)
      BIOS_RegisterRamReset(0xfe);
    else
      BIOS_RegisterRamReset(0xff);
  } else {
    if(cpuIsMultiBoot)
      BIOS_RegisterRamReset(0xfe);
  }

  switch(cpuSaveType) {
  case 0: // automatic
    cpuSramEnabled = true;
    cpuFlashEnabled = true;
    cpuEEPROMEnabled = true;
    cpuEEPROMSensorEnabled = false;
    saveType = gbaSaveType = 0;
    break;
  case 1: // EEPROM
    cpuSramEnabled = false;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = true;
    cpuEEPROMSensorEnabled = false;
    saveType = gbaSaveType = 3;
    // EEPROM usage is automatically detected
    break;
  case 2: // SRAM
    cpuSramEnabled = true;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = false;
    cpuEEPROMSensorEnabled = false;
    cpuSaveGameFunc = sramDelayedWrite; // to insure we detect the write
    saveType = gbaSaveType = 1;
    break;
  case 3: // FLASH
    cpuSramEnabled = false;
    cpuFlashEnabled = true;
    cpuEEPROMEnabled = false;
    cpuEEPROMSensorEnabled = false;
    cpuSaveGameFunc = flashDelayedWrite; // to insure we detect the write
    saveType = gbaSaveType = 2;
    break;
  case 4: // EEPROM+Sensor
    cpuSramEnabled = false;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = true;
    cpuEEPROMSensorEnabled = true;
    // EEPROM usage is automatically detected
    saveType = gbaSaveType = 3;
    break;
  case 5: // NONE
    cpuSramEnabled = false;
    cpuFlashEnabled = false;
    cpuEEPROMEnabled = false;
    cpuEEPROMSensorEnabled = false;
    // no save at all
    saveType = gbaSaveType = 5;
    break;
  }

  ARM_PREFETCH;

  systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

  cpuDmaHack = false;

  //lastTime = systemGetClock();

  SWITicks = 0;
}

void CPUInterrupt()
{
  u32 PC = reg[15].I;
  bool savedState = armState;
  CPUSwitchMode(0x12, true, false);
  reg[14].I = PC;
  if(!savedState)
    reg[14].I += 2;
  reg[15].I = 0x18;
  armState = true;
  armIrqEnable = false;

  armNextPC = reg[15].I;
  reg[15].I += 4;
  ARM_PREFETCH;

  //  if(!holdState)
  biosProtected[0] = 0x02;
  biosProtected[1] = 0xc0;
  biosProtected[2] = 0x5e;
  biosProtected[3] = 0xe5;
}

#define WII_PREFETCH(addr) __asm__ volatile ("dcbt 0, %0" : : "b" (addr))

// -------------------------------------------------------------------------
// Extracting rendering to clear GPR pressure from CPULoop
// -------------------------------------------------------------------------
static __attribute__((noinline)) void CPURenderLine_Wii() {
  (*renderLine)();
  u16 *dest = (u16 *)pix + 242 * (VCOUNT+1);
	for(u32 x = 0; x < 240u;) {
		WII_PREFETCH(&lineMix[x + 16]); // Wii: Fetch 64 bytes ahead into L1 D-Cache
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];

		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];

		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];

		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
		*dest++ = systemColorMap16[lineMix[x++]&0xFFFF];
	}
	*dest++ = 0;
}

// -------------------------------------------------------------------------
// WII OPTIMIZATION: Template unrolled Timer Updates
// Eliminates I-Cache bloat and converts cascade checks into 1-cycle bitwise math.
// -------------------------------------------------------------------------
template <int channel>
static inline void CPUTimerUpdate_T(int clockTicks, int& timerTicks, int& timerReload, int& timerClockReload, u16& timerD, u16 timerCNT, int& timerOverflow) {
  // Compile-time resolution: Timer 0 can never count-up (cascade).
  // For Timers 1-3, check the hardware flag.
  if (channel > 0 && (timerCNT & 4)) {

    // Check if the preceding timer triggered an overflow
    if (timerOverflow & (1 << (channel - 1))) {
      // 1-cycle addition
      u32 nextD = timerD + 1;

      // Branchless overflow evaluation: 1 if rolled over 0xFFFF, else 0
      u32 overflowed = (nextD >> 16);

      // Branchless reload using 0xFFFFFFFF or 0x00000000 mask
      timerD = (u16)(nextD + (timerReload & -overflowed));

      if (overflowed) {
        timerOverflow |= (1 << channel);
        if (channel == 1) soundTimerOverflow(1);
        if (timerCNT & 0x40) {
          // Compile-time shift maps perfectly to 0x08, 0x10, 0x20, 0x40
          IF |= (0x08 << channel);
          WriteReg16(0x202, IF);
        }
      }
      // Compile-time register address calculation (0x100, 0x104, etc.)
      WriteReg16(0x100 + (channel * 4), timerD);
    }

  } else {
    // Normal timing mode
    timerTicks -= clockTicks;

    // Highly predictable branch - favored over heavy bitwise math here
    if (timerTicks <= 0) {
      timerTicks += (0x10000 - timerReload) << timerClockReload;
      timerOverflow |= (1 << channel);

      if (channel == 0) soundTimerOverflow(0);
      if (channel == 1) soundTimerOverflow(1);

      if (timerCNT & 0x40) {
        IF |= (0x08 << channel);
        WriteReg16(0x202, IF);
      }
    }

    // 1-cycle shift and subtract
    timerD = (u16)(0xFFFF - (timerTicks >> timerClockReload));
    WriteReg16(0x100 + (channel * 4), timerD);
  }
}

// -------------------------------------------------------------------------
// Template generation separates execution paths at compile-time,
// keeping L1 I-Cache clean of dead branches.
// -------------------------------------------------------------------------
template <bool EnableCheats>
static void CPULoop_T(int ticks) {
  int clockTicks;
  int timerOverflow = 0;
  cpuTotalTicks = 0;
  cpuBreakLoop = false;
  cpuNextEvent = CPUUpdateTicks();
  if(cpuNextEvent > ticks)
    cpuNextEvent = ticks;

  for(;;) {
    if(!holdState && !SWITicks) {
      if(armState) {
        armOpcodeCount++;
        if (!armExecute()) return;
      } else {
        thumbOpcodeCount++;
        if (!thumbExecute()) return;
      }
      clockTicks = 0;
    } else {
      clockTicks = CPUUpdateTicks();
    }

    cpuTotalTicks += clockTicks;

    if(cpuTotalTicks >= cpuNextEvent) {
      int remainingTicks = cpuTotalTicks - cpuNextEvent;

      if (SWITicks) {
        SWITicks -= clockTicks;
        if (SWITicks < 0) SWITicks = 0;
      }

      clockTicks = cpuNextEvent;
      cpuTotalTicks = 0;

    updateLoop:
      if (IRQTicks) {
        IRQTicks -= clockTicks;
        if (IRQTicks < 0) IRQTicks = 0;
      }

      lcdTicks -= clockTicks;

      if(lcdTicks <= 0) {
        if(DISPSTAT & 1) { // V-BLANK
          if(DISPSTAT & 2) {
            lcdTicks += 1008;
            ++VCOUNT;
            WriteReg16(0x06, VCOUNT);
            DISPSTAT &= 0xFFFD;
            WriteReg16(0x04, DISPSTAT);
            CPUCompareVCOUNT();
          } else {
            lcdTicks += 224;
            DISPSTAT |= 2;
            WriteReg16(0x04, DISPSTAT);
            if(DISPSTAT & 16) {
              IF |= 2;
              WriteReg16(0x202, IF);
            }
          }

          if(VCOUNT >= 228) {
            DISPSTAT &= 0xFFFC;
            WriteReg16(0x04, DISPSTAT);
            VCOUNT = 0;
            WriteReg16(0x06, VCOUNT);
            CPUCompareVCOUNT();
          }
        } else {
          int framesToSkip = speedup ? 9 : systemFrameSkip;

          if(DISPSTAT & 2) {
            ++VCOUNT;
            WriteReg16(0x06, VCOUNT);
            lcdTicks += 1008;
            DISPSTAT &= 0xFFFD;

            if(VCOUNT == 160) {
              ++count;
              systemFrame();

              if((count % 10) == 0) system10Frames(60);
              if(count == 60) count = 0;

              u32 joy = 0;
              if(systemReadJoypads()) joy = systemReadJoypad(-1);
              P1 = 0x03FF ^ (joy & 0x3FF);
              systemUpdateMotionSensor();
              WriteReg16(0x130, P1);

              u16 P1CNT = READ16LE(((u16 *)&ioMem[0x132]));
              if((P1CNT & 0x4000) || stopState) {
                u16 p1 = (0x3FF ^ P1) & 0x3FF;
                if(P1CNT & 0x8000) {
                  if(p1 == (P1CNT & 0x3FF)) { IF |= 0x1000; WriteReg16(0x202, IF); }
                } else {
                  if(p1 & P1CNT) { IF |= 0x1000; WriteReg16(0x202, IF); }
                }
              }

              u32 ext = (joy >> 10);

              if (EnableCheats) {
                remainingTicks += cheatsCheckKeys(P1^0x3FF, ext);
              }

              speedup = (ext & 1) != 0;
              capture = (ext & 2) != 0;

              if(capture && !capturePrevious) {
                ++captureNumber;
                systemScreenCapture(captureNumber);
              }
              capturePrevious = capture;

              DISPSTAT |= 1;
              DISPSTAT &= 0xFFFD;
              WriteReg16(0x04, DISPSTAT);
              if(DISPSTAT & 0x0008) {
                IF |= 1;
                WriteReg16(0x202, IF);
              }
              CPUCheckDMA(1, 0x0f);

              if(frameCount >= framesToSkip) {
                systemDrawScreen();
                frameCount = 0;
              } else {
                ++frameCount;
              }

              if(systemPauseOnFrame()) ticks = 0;
            }

            WriteReg16(0x04, DISPSTAT);
            CPUCompareVCOUNT();

          } else {
            if(frameCount >= framesToSkip) {
                CPURenderLine_Wii(); // Execute decoupled render loop
            }
            DISPSTAT |= 2;
            WriteReg16(0x04, DISPSTAT);
            lcdTicks += 224;
            CPUCheckDMA(2, 0x0f);
            if(DISPSTAT & 16) {
              IF |= 2;
              WriteReg16(0x202, IF);
            }
          }
        }
      }

      soundTicks -= clockTicks;
      if(soundTicks <= 0) {
        psoundTickfn();
        soundTicks += SOUND_CLOCK_TICKS;
      }

      if(!stopState) {
		  // Timers maintained in tight scoping, unrolled via templates
		  // to prevent branch prediction thrashing and stack spilling.
		  if(timer0On) CPUTimerUpdate_T<0>(clockTicks, timer0Ticks, timer0Reload, timer0ClockReload, TM0D, TM0CNT, timerOverflow);
		  if(timer1On) CPUTimerUpdate_T<1>(clockTicks, timer1Ticks, timer1Reload, timer1ClockReload, TM1D, TM1CNT, timerOverflow);
		  if(timer2On) CPUTimerUpdate_T<2>(clockTicks, timer2Ticks, timer2Reload, timer2ClockReload, TM2D, TM2CNT, timerOverflow);
		  if(timer3On) CPUTimerUpdate_T<3>(clockTicks, timer3Ticks, timer3Reload, timer3ClockReload, TM3D, TM3CNT, timerOverflow);
		}

		timerOverflow = 0;

      timerOverflow = 0;

      ticks -= clockTicks;
      cpuNextEvent = CPUUpdateTicks();

      if(cpuDmaTicksToUpdate > 0) {
        if(cpuDmaTicksToUpdate > cpuNextEvent) clockTicks = cpuNextEvent;
        else clockTicks = cpuDmaTicksToUpdate;

        cpuDmaTicksToUpdate -= clockTicks;
        if(cpuDmaTicksToUpdate < 0) cpuDmaTicksToUpdate = 0;
        goto updateLoop;
      }

      if(IF && (IME & 1) && armIrqEnable) {
        int res = IF & IE;
        if(stopState) res &= 0x3080;
        if(res) {
          if (intState) {
            if (!IRQTicks) {
              CPUInterrupt();
              intState = false;
              holdState = false;
              stopState = false;
              holdType = 0;
            }
          } else {
            if (!holdState) {
              intState = true;
              IRQTicks = 7;
              if (cpuNextEvent > IRQTicks) cpuNextEvent = IRQTicks;
            } else {
              CPUInterrupt();
              holdState = false;
              stopState = false;
              holdType = 0;
            }
          }
          if (SWITicks) SWITicks = 0;
        }
      }

      if(remainingTicks > 0) {
        if(remainingTicks > cpuNextEvent) clockTicks = cpuNextEvent;
        else clockTicks = remainingTicks;
        remainingTicks -= clockTicks;
        if(remainingTicks < 0) remainingTicks = 0;
        goto updateLoop;
      }

      if (timerOnOffDelay) applyTimer();

      if(cpuNextEvent > ticks) cpuNextEvent = ticks;

      if(ticks <= 0 || cpuBreakLoop) break;
    }
  }
}

// -------------------------------------------------------------------------
// Main dispatcher. Forwards to the correct compiled template.
// -------------------------------------------------------------------------
void CPULoop(int ticks) {
  bool useCheats = (cheatsEnabled && (mastercode == 0));

  if (useCheats) CPULoop_T<true>(ticks);
  else           CPULoop_T<false>(ticks);
}

struct EmulatedSystem GBASystem = {
  // emuMain
  CPULoop,
  // emuReset
  CPUReset,
  // emuCleanUp
  CPUCleanUp,
  // emuReadMemState
  CPUReadMemState,
  // emuWriteMemState
  CPUWriteMemState,
  // emuUpdateCPSR
  CPUUpdateCPSR,
  // emuHasDebugger
  true,
  // emuCount
  250000
};

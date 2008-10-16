/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vbasupport.cpp
 *
 * VBA support code
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GBA.h"
#include "agbprint.h"
#include "Flash.h"
#include "Port.h"
#include "RTC.h"
#include "Sound.h"
#include "Text.h"
#include "unzip.h"
#include "Util.h"
#include "gb/GB.h"
#include "gb/gbGlobals.h"

#include "vba.h"
#include "fileop.h"
#include "audio.h"
#include "vmmem.h"
#include "pal60.h"
#include "input.h"
#include "video.h"
#include "menudraw.h"

extern "C"
{
#include "tbtime.h"
#include "sdfileio.h"
}

static tb_t start, now;

u32 loadtimeradjust;

int throttle = 100;
u32 throttleLastTime = 0;

static u32 autoFrameSkipLastTime = 0;
static int frameskipadjust = 0;

int vAspect = 0;
int hAspect = 0;

/****************************************************************************
 * VBA Globals
 ***************************************************************************/
int RGB_LOW_BITS_MASK=0x821;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

int systemDebug = 0;
int emulating = 0;

int sensorX = 2047;
int sensorY = 2047;

int systemFrameSkip = 0;
bool systemSoundOn = false;
int systemVerbose = 0;
int cartridgeType = 0;
int srcWidth = 0;
int srcHeight = 0;
int destWidth = 0;
int destHeight = 0;
int srcPitch = 0;
int saveExists = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
u16 systemGbPalette[24];
u16 systemColorMap16[0x10000];
//u32 systemColorMap32[0x10000];
u32 *systemColorMap32 = (u32 *)&systemColorMap16;

struct EmulatedSystem emulator =
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	false,
	0
};

/****************************************************************************
* systemGetClock
*
* Returns number of milliseconds since program start
****************************************************************************/
u32 systemGetClock( void )
{
	mftb(&now);
	return tb_diff_msec(&now, &start) - loadtimeradjust;
}

void systemFrame() {}
void systemScreenCapture(int a) {}
void systemShowSpeed(int speed) {}
void systemGbBorderOn() {}

bool systemPauseOnFrame()
{
	return false;
}

void GC_Sleep(u32 dwMiliseconds)
{
	int nVBlanks = (dwMiliseconds / 16);
	while (nVBlanks-- > 0)
	{
		VIDEO_WaitVSync();
	}
}

void system10Frames(int rate)
{
	if ( cartridgeType == 1 )
		return;

	u32 time = systemGetClock();
	u32 diff = time - autoFrameSkipLastTime;
	int speed = 100;

	if(diff)
		speed = (1000000/rate)/diff;
	/*	  char temp[512];
	sprintf(temp,"Speed: %i",speed);
	MENU_DrawString( -1, 450,temp , 1 );   */

	if(speed >= 98)
	{
		frameskipadjust++;

		if(frameskipadjust >= 3)
		{
			frameskipadjust=0;
			if(systemFrameSkip > 0)
				systemFrameSkip--;
		}
	}
	else
	{
		if(speed  < 80)
			frameskipadjust -= (90 - speed)/5;
		else if(systemFrameSkip < 9)
			frameskipadjust--;

		if(frameskipadjust <= -2)
		{
			frameskipadjust += 2;
			if(systemFrameSkip < 9)
				systemFrameSkip++;
		}
	}

	autoFrameSkipLastTime = time;
}

/****************************************************************************
* System
****************************************************************************/

void systemGbPrint(u8 *data,int pages,int feed,int palette, int contrast) {}
void debuggerOutput(char *, u32) {}
void (*dbgOutput)(char *, u32) = debuggerOutput;
void systemMessage(int num, const char *msg, ...) {}

/****************************************************************************
* Saves
****************************************************************************/

bool LoadBattery(int method, bool silent)
{
	char filepath[1024];
	bool result = false;

	ShowAction ((char*) "Loading...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because we need R/W

	if(method == METHOD_SD || method == METHOD_USB)
	{
		ChangeFATInterface(method, NOTSILENT);
		sprintf (filepath, "%s/%s/%s.sav", ROOTFATDIR, GCSettings.SaveFolder, ROMFilename);
		result = emulator.emuReadBattery(filepath);
	}

	if(!result && !silent)
		WaitPrompt ((char*) "Save file not found");

	return result;
}

bool SaveBattery(int method, bool silent)
{
	char filepath[1024];
	bool result = false;

	ShowAction ((char*) "Saving...");

	if(method == METHOD_AUTO)
			method = autoSaveMethod(); // we use 'Save' because we need R/W

	if(method == METHOD_SD || method == METHOD_USB)
	{
		ChangeFATInterface(method, NOTSILENT);
		sprintf (filepath, "%s/%s/%s.sav", ROOTFATDIR, GCSettings.SaveFolder, ROMFilename);
		result = emulator.emuWriteBattery(filepath);
	}

	if(!result && !silent)
		WaitPrompt ((char*) "Save failed");

	return result;
}

bool LoadState(int method, bool silent)
{
	char filepath[1024];
	bool result = false;

	ShowAction ((char*) "Loading...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because we need R/W

	if(method == METHOD_SD || method == METHOD_USB)
	{
		ChangeFATInterface(method, NOTSILENT);
		sprintf (filepath, "%s/%s/%s.sgm", ROOTFATDIR, GCSettings.SaveFolder, ROMFilename);
		result = emulator.emuReadState(filepath);
	}

	if(!result && !silent)
		WaitPrompt ((char*) "State file not found");

	return result;
}

bool SaveState(int method, bool silent)
{
	char filepath[1024];
	bool result = false;

	ShowAction ((char*) "Saving...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because we need R/W

	if(method == METHOD_SD || method == METHOD_USB)
	{
		ChangeFATInterface(method, NOTSILENT);
		sprintf (filepath, "%s/%s/%s.sgm", ROOTFATDIR, GCSettings.SaveFolder, ROMFilename);
		result = emulator.emuWriteState(filepath);
	}

	if(!result && !silent)
		WaitPrompt ((char*) "Save failed");

	return result;
}

/****************************************************************************
* Sound
****************************************************************************/

void systemWriteDataToSoundBuffer()
{
	MIXER_AddSamples((u8 *)soundFinalWave, (cartridgeType == 1));
}

bool systemSoundInit()
{
	return true;
}

bool systemCanChangeSoundQuality()
{
	return true;
}

void systemSoundPause() {}
void systemSoundResume() {}
void systemSoundReset() {}
void systemSoundShutdown() {}

/****************************************************************************
* systemReadJoypads
****************************************************************************/
bool systemReadJoypads()
{
	return true;
}

u32 systemReadJoypad(int which)
{
	return GetJoy();
}

/****************************************************************************
* systemDrawScreen
****************************************************************************/
void systemDrawScreen()
{
	// GB / GBC Have oodles of time - so sync on VSync
	GX_Render( srcWidth, srcHeight, pix, srcPitch );

	#ifdef HW_RVL
	VIDEO_WaitVSync ();
	#else
	if ( cartridgeType == 1 )
	{
		VIDEO_WaitVSync();
	}
	#endif
}

int loadVBAROM(char filename[])
{
	cartridgeType = 0;
	srcWidth = 0;
	srcHeight = 0;
	destWidth = 0;
	destHeight = 0;
	srcPitch = 0;

	IMAGE_TYPE type = utilFindType(filename);

	switch( type )
	{
		case IMAGE_GBA:
		//WaitPrompt("GameBoy Advance Image");
		cartridgeType = 2;
		emulator = GBASystem;
		srcWidth = 240;
		srcHeight = 160;
		VMCPULoadROM(filename);
		// Actual Visual Aspect is 1.57
		hAspect = 70;
		vAspect = 46;
		srcPitch = 484;
		soundQuality = 2;
		soundBufferLen = 1470;
		cpuSaveType = 0;
		break;

		case IMAGE_GB:
		//WaitPrompt("GameBoy Image");
		cartridgeType = 1;
		emulator = GBSystem;
		srcWidth = 160;
		srcHeight = 144;
		gbLoadRom(filename);
		// Actual physical aspect is 1.0
		hAspect = 60;
		vAspect = 46;
		srcPitch = 324;
		soundQuality = 1;
		soundBufferLen = 1470 * 2;
		break;

		default:
		WaitPrompt((char *)"Unknown Image");
		return 0;
		break;
	}

	// Set defaults
	flashSetSize(0x10000);
	rtcEnable(true);
	agbPrintEnable(false);
	soundOffFlag = false;
	soundLowPass = true;

	// Setup GX
	GX_Render_Init( srcWidth, srcHeight, hAspect, vAspect );

	if ( cartridgeType == 1 )
	{
		gbSoundReset();
		gbSoundSetQuality(soundQuality);
	}
	else
	{
		soundSetQuality(soundQuality);
		CPUInit("/VBA/BIOS/BIOS.GBA", 1);
		CPUReset();
	}

	soundVolume = 0;
	systemSoundOn = true;

	soundInit();

	emulating = 1;

	// Start system clock
	mftb(&start);

	return 1;
}

/****************************************************************************
* EEPROM
****************************************************************************/
int systemGetSensorX()
{
	return sensorX;
}

int systemGetSensorY()
{
	return sensorY;
}

void systemUpdateMotionSensor() {}

/****************************************************************************
* Palette
****************************************************************************/

void InitialisePalette()
{
	int i;
	// Build GBPalette
	for( i = 0; i < 24; )
	{
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}
	// Set palette etc - Fixed to RGB565
	systemColorDepth = 16;
	systemRedShift = 11;
	systemGreenShift = 6;
	systemBlueShift = 0;
	for(i = 0; i < 0x10000; i++)
	{
		systemColorMap16[i] =
			((i & 0x1f) << systemRedShift) |
			(((i & 0x3e0) >> 5) << systemGreenShift) |
			(((i & 0x7c00) >> 10) << systemBlueShift);
	}
}

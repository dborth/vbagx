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

#include "agb/GBA.h"
#include "agb/agbprint.h"
#include "Flash.h"
#include "Port.h"
#include "RTC.h"
#include "Sound.h"
#include "unzip.h"
#include "Util.h"
#include "dmg/GB.h"
#include "dmg/gbGlobals.h"
//#include "dmg/gbSound.h"

#include "vba.h"
#include "fileop.h"
#include "dvd.h"
#include "smbop.h"
#include "memcardop.h"
#include "audio.h"
#include "vmmem.h"
#include "input.h"
#include "video.h"
#include "menudraw.h"
#include "gcunzip.h"

extern "C"
{
#include "tbtime.h"
}

static tb_t start, now;

u32 loadtimeradjust;

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
u32 *systemColorMap32 = NULL;

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
/*
void GC_Sleep(u32 dwMiliseconds)
{
	int nVBlanks = (dwMiliseconds / 16);
	while (nVBlanks-- > 0)
	{
		VIDEO_WaitVSync();
	}
}
*/
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
void debuggerOutput(const char *s, u32 addr) {}
void (*dbgOutput)(const char *s, u32 addr) = debuggerOutput;
void systemMessage(int num, const char *msg, ...) {}

bool MemCPUReadBatteryFile(char * membuffer, int size)
{
	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(size == 512 || size == 0x2000)
	{
		memcpy(eepromData, membuffer, size);
	}
	else
	{
		if(size == 0x20000)
		{
			memcpy(flashSaveMemory, membuffer, 0x20000);
			flashSetSize(0x20000);
		}
		else
		{
			memcpy(flashSaveMemory, membuffer, 0x10000);
			flashSetSize(0x10000);
		}
	}
	return true;
}

extern int gbaSaveType;

int MemCPUWriteBatteryFile(char * membuffer)
{
	int result = 0;
	if(gbaSaveType == 0)
	{
		if(eepromInUse)
			gbaSaveType = 3;
		else
			switch(saveType)
			{
			case 1:
				gbaSaveType = 1;
				break;
			case 2:
				gbaSaveType = 2;
				break;
			}
	}

	if((gbaSaveType) && (gbaSaveType!=5))
	{
		// only save if Flash/Sram in use or EEprom in use
		if(gbaSaveType != 3)
		{
			if(gbaSaveType == 2)
			{
				memcpy(membuffer, flashSaveMemory, flashSize);
				result = flashSize;
			}
			else
			{
				memcpy(membuffer, flashSaveMemory, 0x10000);
				result = 0x10000;
			}
		}
		else
		{
			memcpy(membuffer, eepromData, eepromSize);
			result = eepromSize;
		}
	}
	return result;
}

/****************************************************************************
* SetFileBytesWritten
* Sets the # of bytes written into a file
* Used by GBA.cpp and GB.cpp
****************************************************************************/

void SetFileBytesWritten(int bytes)
{
	//datasize = bytes;
}

/****************************************************************************
* LoadBatteryOrState
* Load Battery/State file into memory
* action = 0 - Load battery
* action = 1 - Load state
****************************************************************************/

bool LoadBatteryOrState(int method, int action, bool silent)
{
	char filepath[1024];
	bool result = false;
	int offset = 0;
	char ext[4];

	if(action == 0)
		sprintf(ext, "sav");
	else
		sprintf(ext, "sgm");

	ShowAction ((char*) "Loading...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because we need R/W

	AllocSaveBuffer();

	// load the file into savebuffer

	if(method == METHOD_SD || method == METHOD_USB)
	{
		if(ChangeFATInterface(method, NOTSILENT))
		{
			sprintf (filepath, "%s/%s/%s.%s", ROOTFATDIR, GCSettings.SaveFolder, ROMFilename, ext);
			offset = LoadBufferFromFAT (filepath, silent);
		}
	}
	else if(method == METHOD_SMB)
	{
		sprintf (filepath, "%s/%s.%s", GCSettings.SaveFolder, ROMFilename, ext);
		offset = LoadBufferFromSMB (filepath, silent);
	}
	else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		sprintf (filepath, "%s.%s", ROMFilename, ext);

		if(method == METHOD_MC_SLOTA)
			offset = LoadBufferFromMC (savebuffer, CARD_SLOTA, filepath, silent);
		else
			offset = LoadBufferFromMC (savebuffer, CARD_SLOTB, filepath, silent);
	}

	// load savebuffer into VBA memory
	if (offset > 0)
	{
		if(action == 0)
		{
			if(cartridgeType == 1)
				result = MemgbReadBatteryFile((char *)savebuffer, offset);
			else
				result = MemCPUReadBatteryFile((char *)savebuffer, offset);
		}
		else
		{
			result = emulator.emuReadMemState((char *)savebuffer, offset);
		}
	}

	FreeSaveBuffer();

	if(!silent && !result)
	{
		if(offset == 0)
		{
			if(action == 0)
				WaitPrompt ((char*) "Save file not found");
			else
				WaitPrompt ((char*) "State file not found");
		}
		else
		{
			if(action == 0)
				WaitPrompt ((char*) "Invalid save file");
			else
				WaitPrompt ((char*) "Invalid state file");
		}
	}
	return result;
}


/****************************************************************************
* SaveBatteryOrState
* Save Battery/State file into memory
* action = 0 - Save battery
* action = 1 - Save state
****************************************************************************/

bool SaveBatteryOrState(int method, int action, bool silent)
{
	char filepath[1024];
	bool result = false;
	int offset = 0;
	char ext[4];
	int datasize = 0; // we need the actual size of the data written

	if(action == 0)
		sprintf(ext, "sav");
	else
		sprintf(ext, "sgm");

	ShowAction ((char*) "Saving...");

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because we need R/W

	AllocSaveBuffer();

	// put VBA memory into savebuffer, sets datasize to size of memory written
	if(action == 0)
	{
		if(cartridgeType == 1)
			datasize = MemgbWriteBatteryFile((char *)savebuffer);
		else
			datasize = MemCPUWriteBatteryFile((char *)savebuffer);
	}
	else
	{
		bool written = emulator.emuWriteMemState((char *)savebuffer, SAVEBUFFERSIZE);
		// we really should set datasize to the exact memory size written
		// but instead we'll set it at 128K - although much of it will go unused
		if(written)
			datasize = (512*256);
	}

	// write savebuffer into file
	if(datasize > 0)
	{
		if(method == METHOD_SD || method == METHOD_USB)
		{
			if(ChangeFATInterface(method, NOTSILENT))
			{
				sprintf (filepath, "%s/%s/%s.%s", ROOTFATDIR, GCSettings.SaveFolder, ROMFilename, ext);
				offset = SaveBufferToFAT (filepath, datasize, silent);
			}
		}
		else if(method == METHOD_SMB)
		{
			sprintf (filepath, "%s/%s.%s", GCSettings.SaveFolder, ROMFilename, ext);
			offset = SaveBufferToSMB (filepath, datasize, silent);
		}
		else if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
		{
			sprintf (filepath, "%s.%s", ROMFilename, ext);

			if(method == METHOD_MC_SLOTA)
				offset = SaveBufferToMC (savebuffer, CARD_SLOTA, filepath, datasize, silent);
			else
				offset = SaveBufferToMC (savebuffer, CARD_SLOTB, filepath, datasize, silent);
		}

		if(offset > 0)
		{
			if(!silent)
				WaitPrompt ((char*) "Save successful");
			result = true;
		}
	}
	else
	{
		if(!silent)
			WaitPrompt((char *)"No data to save!");
	}

	FreeSaveBuffer();

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

extern bool gbUpdateSizes();
bool LoadGBROM(int method)
{
	// cleanup GB memory
	if(gbRom != NULL)
		gbCleanUp();

	gbRom = (u8 *)malloc(1024*1024*4); // allocate 4 MB to GB ROM

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	switch (method)
	{
		case METHOD_SD:
		case METHOD_USB:
		gbRomSize = LoadFATFile ((char *)gbRom, 0);
		break;

		case METHOD_DVD:
		gbRomSize = LoadDVDFile ((unsigned char *)gbRom, 0);
		break;

		case METHOD_SMB:
		gbRomSize = LoadSMBFile ((char *)gbRom, 0);
		break;
	}

	if(!gbRom)
		return false;

	return gbUpdateSizes();
}

bool LoadVBAROM(int method)
{
	int type = 0;
	bool loaded = false;

	// image type (checks file extension)
	if(utilIsGBAImage(filelist[selection].filename))
		type = 2;
	else if(utilIsGBImage(filelist[selection].filename))
		type = 1;
	else if(utilIsZipFile(filelist[selection].filename))
	{
		// we need to check the file extension of the first file in the archive
		char * zippedFilename = GetFirstZipFilename (method);

		if(strlen(zippedFilename) > 0)
		{
			if(utilIsGBAImage(zippedFilename))
				type = 2;
			else if(utilIsGBImage(zippedFilename))
				type = 1;
		}
	}

	// leave before we do anything
	if(type != 1 && type != 2)
	{
		WaitPrompt((char *)"Unknown game image!");
		return false;
	}

	cartridgeType = 0;
	srcWidth = 0;
	srcHeight = 0;
	destWidth = 0;
	destHeight = 0;
	srcPitch = 0;

	switch( type )
	{
		case 2:
		//WaitPrompt("GameBoy Advance Image");
		cartridgeType = 2;
		emulator = GBASystem;
		srcWidth = 240;
		srcHeight = 160;
		loaded = VMCPULoadROM(method);
		// Actual Visual Aspect is 1.57
		hAspect = 70;
		vAspect = 46;
		srcPitch = 484;
		soundQuality = 2;
		soundBufferLen = 1470;
		cpuSaveType = 0;
		break;

		case 1:
		//WaitPrompt("GameBoy Image");
		cartridgeType = 1;
		emulator = GBSystem;
		srcWidth = 160;
		srcHeight = 144;
		loaded = LoadGBROM(method);
		// Actual physical aspect is 1.0
		hAspect = 60;
		vAspect = 46;
		srcPitch = 324;
		soundQuality = 1;
		soundBufferLen = 1470 * 2;
		break;
	}

	if(!loaded)
	{
		WaitPrompt((char *)"Error loading game!");
		return false;
	}
	else
	{
		// Set defaults
		flashSetSize(0x20000); // 128K saves
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

		// reset frameskip variables
		autoFrameSkipLastTime = frameskipadjust = systemFrameSkip = 0;

		// Start system clock
		mftb(&start);

		return true;
	}
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

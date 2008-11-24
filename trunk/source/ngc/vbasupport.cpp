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

#include "unzip.h"
#include "Util.h"
#include "Flash.h"
#include "Patch.h"
#include "Port.h"
#include "RTC.h"
#include "Sound.h"
#include "Cheats.h"
#include "agb/GBA.h"
#include "agb/agbprint.h"
#include "dmg/gb.h"
#include "dmg/gbGlobals.h"
#include "dmg/gbCheats.h"
#include "dmg/gbSound.h"

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
#include "gamesettings.h"
#include "images/saveicon.h"

extern "C"
{
#include "tbtime.h"
long long gettime();
u32 diff_usec(long long start,long long end);
}

static tb_t start, now;

u32 loadtimeradjust;

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
int systemVerbose = 0;
int cartridgeType = 0;
int srcWidth = 0;
int srcHeight = 0;
int srcPitch = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
u16 systemGbPalette[24];
u16 systemColorMap16[0x10000];
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

static u32 lastTime = 0;
#define RATE60HZ 166666.67 // 1/6 second or 166666.67 usec

void system10Frames(int rate)
{
	u32 time = gettime();
	u32 diff = diff_usec(lastTime, time);

	// expected diff - actual diff
	u32 timeOff = RATE60HZ - diff;

	if(timeOff > 0 && timeOff < 100000) // we're running ahead!
		usleep(timeOff); // let's take a nap
	else
		timeOff = 0; // timeoff was not valid

	int speed = (RATE60HZ/diff)*100;

	if (cartridgeType == 2) // GBA games require frameskipping
	{
		// consider increasing skip
		if(speed < 98)
			systemFrameSkip += 1;
		else if(speed < 80)
			systemFrameSkip += 2;
		else if(speed < 70)
			systemFrameSkip += 3;
		else if(speed < 60)
			systemFrameSkip += 4;

		// consider decreasing skip
		else if(speed > 185)
			systemFrameSkip -= 3;
		else if(speed > 145)
			systemFrameSkip -= 2;
		else if(speed > 125)
			systemFrameSkip -= 1;

		// correct invalid frame skip values
		if(systemFrameSkip > 20)
			systemFrameSkip = 20;
		else if(systemFrameSkip < 0)
			systemFrameSkip = 0;
	}
	lastTime = gettime();
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
* LoadBatteryOrState
* Load Battery/State file into memory
* action = FILE_SRAM - Load battery
* action = FILE_SNAPSHOT - Load state
****************************************************************************/

bool LoadBatteryOrState(int method, int action, bool silent)
{
	char filepath[1024];
	bool result = false;
	int offset = 0;

	if(method == METHOD_AUTO)
		method = autoSaveMethod(); // we use 'Save' because we need R/W

	if(!MakeFilePath(filepath, action, method))
		return false;

	ShowAction ((char*) "Loading...");

	AllocSaveBuffer();

	// load the file into savebuffer
	offset = LoadFile(filepath, method, silent);

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		// skip save icon and comments for Memory Card saves
		int skip = sizeof (saveicon);
		skip += 64; // sizeof savecomment
		memmove(savebuffer, savebuffer+skip, offset-skip);
		offset -= skip;
	}

	// load savebuffer into VBA memory
	if (offset > 0)
	{
		if(action == FILE_SRAM)
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
			if(action == FILE_SRAM)
				WaitPrompt ((char*) "Save file not found");
			else
				WaitPrompt ((char*) "State file not found");
		}
		else
		{
			if(action == FILE_SRAM)
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
	int datasize = 0; // we need the actual size of the data written

	if(method == METHOD_AUTO)
		method = autoSaveMethod();

	if(!MakeFilePath(filepath, action, method))
		return false;

	ShowAction ((char*) "Saving...");

	AllocSaveBuffer();

	// add save icon and comments for Memory Card saves
	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		char savecomment[2][32];
		char savetype[10];

		offset = sizeof (saveicon);

		// Copy in save icon
		memcpy (savebuffer, saveicon, offset);

		// And the comments
		if(action == FILE_SRAM)
			sprintf(savetype, "SRAM");
		else
			sprintf(savetype, "Freeze");

		sprintf (savecomment[0], "%s %s", VERSIONSTR, savetype);
		strncpy(savecomment[1], ROMFilename, 31); // truncate filename to 31 chars
		savecomment[1][31] = 0; // make sure last char is null byte
		memcpy (savebuffer + offset, savecomment, 64);
		offset += 64;
	}

	// put VBA memory into savebuffer, sets datasize to size of memory written
	if(action == FILE_SRAM)
	{
		if(cartridgeType == 1)
			datasize = MemgbWriteBatteryFile((char *)savebuffer+offset);
		else
			datasize = MemCPUWriteBatteryFile((char *)savebuffer+offset);
	}
	else
	{
		bool written = emulator.emuWriteMemState((char *)savebuffer+offset, SAVEBUFFERSIZE-offset);
		// we need to set datasize to the exact memory size written
		// but emuWriteMemState doesn't return that for us
		// so instead we'll find the end of the save the old fashioned way
		if(written)
		{
			datasize = (1024*192); // we'll start at 192K - no save should be larger
			char check = savebuffer[datasize];
			while(check == 0)
			{
				datasize -= 16384;
				check = savebuffer[datasize];
			}
			datasize += 16384;
			check = savebuffer[datasize];
			while(check == 0)
			{
				datasize -= 1024;
				check = savebuffer[datasize];
			}
			datasize += 1024;
			check = savebuffer[datasize];
			while(check == 0)
			{
				datasize -= 64;
				check = savebuffer[datasize];
			}
			datasize += 64;
			check = savebuffer[datasize];
			while(check == 0)
			{
				datasize -= 1;
				check = savebuffer[datasize];
			}
			datasize += 2; // include last byte AND a null byte
		}
	}

	// write savebuffer into file
	if(datasize > 0)
	{
		offset = SaveFile(filepath, datasize, method, silent);

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
	ResetAudio();
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
	return GetJoy(which);
}

/****************************************************************************
* Motion/Tilt sensor
* Used for games like:
* - Yoshi's Universal Gravitation
* - Kirby's Tilt-N-Tumble
* - Wario Ware Twisted!
*
****************************************************************************/
int systemGetSensorX()
{
	return sensorX;
}

int systemGetSensorY()
{
	return sensorY;
}

void systemUpdateMotionSensor()
{
	int chan = 0; // first wiimote
/*
	WPADData *Data = WPAD_Data(chan);
	WPADData data = *Data;

	WPAD_Orientation(chan, &data.orient);
	WPAD_GForce(chan, &data.gforce);
	WPAD_Accel(chan, &data.accel);

	//rotz = (float)orient.roll;
	//roty = (float)orient.pitch;
	//rotx = (float)orient.yaw;

	//rotz = (float)(2.0*3.14159*((int)orient.roll/360.0));//Removing extra stuff fails too
	//roty = (float)(2.0*3.14159*((int)orient.pitch/360.0));
	//rotx = (float)(2.0*3.14159*((int)orient.yaw/360.0));

	//rotz = (float)accel.z;
	//roty = (float)accel.y;
	//rotx = (float)accel.x;

	//rotz = 200-(gforce.z*50);//Even doing this without the extra stuff fails
	//roty = 200-(gforce.y*50);
	//rotx = 200-(gforce.x*50);

	printf("ACCEL X %d             \n", (int)Data->accel.x);
	printf("ACCEL Y %d             \n", (int)Data->accel.y);
	printf("ACCEL Z %d             \n", (int)Data->accel.z);

	printf("HORIENT ROLL %1.3f             ", (float)Data->orient.roll);
	printf("HORIENT PITCH %1.3f             ", (float)Data->orient.pitch);
	printf("HORIENT YAW %1.3f             ", (float)Data->orient.yaw);

	printf("GFORCE X %1.3f             \n", (float)data.gforce.x);
	printf("GFORCE Y %1.3f             \n", (float)data.gforce.y);
	printf("GFORCE Z %1.3f             \n", (float)data.gforce.z);
*/
/*
if(sdlMotionButtons[KEY_LEFT]) {
    sensorX += 3;
    if(sensorX > 2197)
      sensorX = 2197;
    if(sensorX < 2047)
      sensorX = 2057;
  } else if(sdlMotionButtons[KEY_RIGHT]) {
    sensorX -= 3;
    if(sensorX < 1897)
      sensorX = 1897;
    if(sensorX > 2047)
      sensorX = 2037;
  } else if(sensorX > 2047) {
    sensorX -= 2;
    if(sensorX < 2047)
      sensorX = 2047;
  } else {
    sensorX += 2;
    if(sensorX > 2047)
      sensorX = 2047;
  }

  if(sdlMotionButtons[KEY_UP]) {
    sensorY += 3;
    if(sensorY > 2197)
      sensorY = 2197;
    if(sensorY < 2047)
      sensorY = 2057;
  } else if(sdlMotionButtons[KEY_DOWN]) {
    sensorY -= 3;
    if(sensorY < 1897)
      sensorY = 1897;
    if(sensorY > 2047)
      sensorY = 2037;
  } else if(sensorY > 2047) {
    sensorY -= 2;
    if(sensorY < 2047)
      sensorY = 2047;
  } else {
    sensorY += 2;
    if(sensorY > 2047)
      sensorY = 2047;
  }
 */
}

/****************************************************************************
* systemDrawScreen
****************************************************************************/
void systemDrawScreen()
{
	GX_Render( srcWidth, srcHeight, pix, srcPitch );
}

/****************************************************************************
 * ApplyPerImagePreferences
 * Apply game specific settings, originally from vba-over.ini
 ***************************************************************************/

static void ApplyPerImagePreferences()
{
	// look for matching game setting
	int snum = -1;

	for(int i=0; i < gameSettingsCount; i++)
	{
		if(gameSettings[i].gameID[0] == rom[0xac] &&
			gameSettings[i].gameID[1] == rom[0xad] &&
			gameSettings[i].gameID[2] == rom[0xae] &&
			gameSettings[i].gameID[3] == rom[0xaf])
		{
			snum = i;
			break;
		}
	}

	// match found!
	if(snum >= 0)
	{
		if(gameSettings[snum].rtcEnabled >= 0)
			rtcEnable(gameSettings[snum].rtcEnabled);
		if(gameSettings[snum].flashSize > 0)
			flashSetSize(gameSettings[snum].flashSize);
		if(gameSettings[snum].saveType >= 0)
			cpuSaveType = gameSettings[snum].saveType;
		if(gameSettings[snum].mirroringEnabled >= 0)
			mirroringEnable = gameSettings[snum].mirroringEnabled;
	}
}

void LoadPatch(int method)
{
	int patchsize = 0;
	int patchtype;

	ShowAction((char *)"Loading patch...");

	AllocSaveBuffer ();

	char patchpath[3][512];
	memset(patchpath, 0, sizeof(patchpath));
	sprintf(patchpath[0], "%s/%s.ips",currentdir,ROMFilename);
	sprintf(patchpath[1], "%s/%s.ups",currentdir,ROMFilename);
	sprintf(patchpath[2], "%s/%s.ppf",currentdir,ROMFilename);

	for(patchtype=0; patchtype<3; patchtype++)
	{
		patchsize = LoadFile(patchpath[patchtype], method, SILENT);

		if(patchsize)
			break;
	}

	if(patchsize > 0)
	{
		// create memory file
		MFILE * mf = memfopen((char *)savebuffer, patchsize);

		if(cartridgeType == 1)
		{
			if(patchtype == 0)
				patchApplyIPS(mf, &gbRom, &gbRomSize);
			else if(patchtype == 1)
				patchApplyUPS(mf, &gbRom, &gbRomSize);
			else
				patchApplyPPF(mf, &gbRom, &gbRomSize);
		}
		else
		{
			if(patchtype == 0)
				patchApplyIPS(mf, &rom, &GBAROMSize);
			else if(patchtype == 1)
				patchApplyUPS(mf, &rom, &GBAROMSize);
			else
				patchApplyPPF(mf, &rom, &GBAROMSize);
		}

		memfclose(mf); // close memory file
	}

	FreeSaveBuffer ();
}

extern bool gbUpdateSizes();

bool LoadGBROM(int method)
{
	gbRom = (u8 *)malloc(1024*1024*4); // allocate 4 MB to GB ROM
	bios = (u8 *)calloc(1,0x100);

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(!inSz)
	{
		char filepath[1024];

		if(!MakeFilePath(filepath, FILE_ROM, method))
			return false;

		gbRomSize = LoadFile ((char *)gbRom, filepath, filelist[selection].length, method, NOTSILENT);
	}
	else
	{
		switch (method)
		{
			case METHOD_SD:
			case METHOD_USB:
				gbRomSize = LoadFATSzFile(szpath, (unsigned char *)gbRom);
				break;
			case METHOD_DVD:
				gbRomSize = SzExtractFile(filelist[selection].offset, (unsigned char *)gbRom);
				break;
			case METHOD_SMB:
				gbRomSize = LoadSMBSzFile(szpath,  (unsigned char *)gbRom);
				break;
		}
	}

	if(gbRomSize <= 0)
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
	srcPitch = 0;

	VMClose(); // cleanup GBA memory
	gbCleanUp(); // cleanup GB memory

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
			soundBufferLen = 736 * 2;
			cpuSaveType = 0;
			break;

		case 1:
			//WaitPrompt("GameBoy Image");
			cartridgeType = 1;
			emulator = GBSystem;

			gbBorderOn = 0; // GB borders always off

			if(gbBorderOn)
			{
				srcWidth = 256;
				srcHeight = 224;
				gbBorderLineSkip = 256;
				gbBorderColumnSkip = 48;
				gbBorderRowSkip = 40;
			}
			else
			{
				srcWidth = 160;
				srcHeight = 144;
				gbBorderLineSkip = 160;
				gbBorderColumnSkip = 0;
				gbBorderRowSkip = 0;
			}

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
		// Setup GX
		GX_Render_Init( srcWidth, srcHeight, hAspect, vAspect );

		if (cartridgeType == 1)
		{
			gbGetHardwareType();

			// used for the handling of the gb Boot Rom
			//if (gbHardware & 5)
			//gbCPUInit(gbBiosFileName, useBios);

			LoadPatch(method);

			gbSoundReset();
			gbSoundSetQuality(soundQuality);
			gbSoundSetDeclicking(true);
			gbReset();
		}
		else
		{
			// Set defaults
			cpuSaveType = 0; // automatic
			flashSetSize(0x10000); // 64K saves
			rtcEnable(false);
			agbPrintEnable(false);
			mirroringEnable = false;

			// Apply preferences specific to this game
			ApplyPerImagePreferences();
			doMirroring(mirroringEnable);

			soundReset();
			soundSetQuality(soundQuality);
			CPUInit("BIOS.GBA", 1);
			LoadPatch(method);
			CPUReset();
		}

		soundInit();

		emulating = 1;

		// reset frameskip variables
		lastTime = systemFrameSkip = 0;

		// Start system clock
		mftb(&start);

		return true;
	}
}

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

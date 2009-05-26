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
#include <wiiuse/wpad.h>
#include <malloc.h>
#include <ogc/lwp_watchdog.h>

#include "pngu/pngu.h"

#include "unzip.h"
#include "Util.h"
#include "common/Port.h"
#include "gba/Flash.h"
#include "common/Patch.h"
#include "gba/RTC.h"
#include "gba/Sound.h"
#include "gba/Cheats.h"
#include "gba/GBA.h"
#include "gba/agbprint.h"
#include "gb/gb.h"
#include "gb/gbGlobals.h"
#include "gb/gbCheats.h"
#include "gb/gbSound.h"

#include "vba.h"
#include "fileop.h"
#include "filebrowser.h"
#include "dvd.h"
#include "memcardop.h"
#include "audio.h"
#include "vmmem.h"
#include "input.h"
#include "gameinput.h"
#include "video.h"
#include "menu.h"
#include "gcunzip.h"
#include "gamesettings.h"

static u32 start;
int cartridgeType = 0;
u32 RomIdCode;
int SunBars = 3;
bool TiltSideways = false;

/****************************************************************************
 * VBA Globals
 ***************************************************************************/

int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

int systemDebug = 0;
int emulating = 0;

int systemFrameSkip = 0;
int systemVerbose = 0;

int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
u16 systemGbPalette[24];
u16 systemColorMap16[0x10000];
u32 *systemColorMap32 = NULL;

void gbSetPalette(u32 RRGGBB[]);
bool StartColorizing();
void StopColorizing();
extern bool ColorizeGameboy;
extern u16 systemMonoPalette[14];
void gbSetBgPal(u8 WhichPal, u32 bright, u32 medium, u32 dark, u32 black=0x000000);
void gbSetSpritePal(u8 WhichPal, u32 bright, u32 medium, u32 dark);

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
	u32 now = gettime();
	return diff_usec(start, now) / 1000;
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

bool LoadBatteryOrState(char * filepath, int method, int action, bool silent)
{
	bool result = false;
	int offset = 0;

	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent); // we use 'Save' because we need R/W

	if(method == METHOD_AUTO)
		return false;

	AllocSaveBuffer();

	// load the file into savebuffer
	offset = LoadFile(filepath, method, silent);

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
				ErrorPrompt ("Save file not found");
			else
				ErrorPrompt ("State file not found");
		}
		else
		{
			if(action == FILE_SRAM)
				ErrorPrompt ("Invalid save file");
			else
				ErrorPrompt ("Invalid state file");
		}
	}
	return result;
}

bool LoadBatteryOrStateAuto(int method, int action, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	char filepath[MAXPATHLEN];
	char fullpath[MAXPATHLEN];
	char filepath2[MAXPATHLEN];
	char fullpath2[MAXPATHLEN];

	if(!MakeFilePath(filepath, action, method, ROMFilename, 0))
		return false;

	if (action==FILE_SRAM)
	{
		if (LoadBatteryOrState(filepath, method, action, SILENT))
			return true;

		// look for file with no number or Auto appended
		if(!MakeFilePath(filepath2, action, method, ROMFilename, -1))
			return false;

		if(LoadBatteryOrState(filepath2, method, action, silent))
		{
			// rename this file - append Auto
			sprintf(fullpath, "%s%s", rootdir, filepath); // add device to path
			sprintf(fullpath2, "%s%s", rootdir, filepath2); // add device to path
			rename(fullpath2, fullpath); // rename file (to avoid duplicates)
			return true;
		}
		return false;
	}
	else
	{
		return LoadBatteryOrState(filepath, method, action, silent);
	}
}

/****************************************************************************
* SaveBatteryOrState
* Save Battery/State file into memory
* action = 0 - Save battery
* action = 1 - Save state
****************************************************************************/

bool SaveBatteryOrState(char * filepath, int method, int action, bool silent)
{
	bool result = false;
	int offset = 0;
	int datasize = 0; // we need the actual size of the data written
	int imgSize = 0; // image screenshot bytes written

	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	// save screenshot - I would prefer to do this from gameScreenTex
	if(action == FILE_SNAPSHOT && gameScreenTex2 != NULL && method != METHOD_MC_SLOTA && method != METHOD_MC_SLOTB)
	{
		AllocSaveBuffer ();

		IMGCTX pngContext = PNGU_SelectImageFromBuffer(savebuffer);

		if (pngContext != NULL)
		{
			imgSize = PNGU_EncodeFromGXTexture(pngContext, screenwidth, screenheight, gameScreenTex2, 0);
			PNGU_ReleaseImageContext(pngContext);
		}

		if(imgSize > 0)
		{
			char screenpath[1024];
			strncpy(screenpath, filepath, 1024);
			screenpath[strlen(screenpath)-4] = 0;
			sprintf(screenpath, "%s.png", screenpath);
			SaveFile(screenpath, imgSize, method, silent);
		}

		FreeSaveBuffer ();
	}

	AllocSaveBuffer();

	// set comments for Memory Card saves
	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		char savecomments[2][32];
		char savetype[10];
		memset(savecomments, 0, 64);

		if(action == FILE_SRAM)
			sprintf(savetype, "SRAM");
		else
			sprintf(savetype, "Snapshot");

		sprintf (savecomments[0], "%s %s", APPNAME, savetype);
		snprintf (savecomments[1], 32, ROMFilename);
		SetMCSaveComments(savecomments);
	}

	// put VBA memory into savebuffer, sets datasize to size of memory written
	if(action == FILE_SRAM)
	{
		if(cartridgeType == 1)
			datasize = MemgbWriteBatteryFile((char *)savebuffer);
		else
			datasize = MemCPUWriteBatteryFile((char *)savebuffer);
	}
	else
	{
		bool written = emulator.emuWriteMemState((char *)savebuffer, SAVEBUFFERSIZE);
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
				InfoPrompt ("Save successful");
			result = true;
		}
	}
	else
	{
		if(!silent)
			InfoPrompt("No data to save!");
	}

	FreeSaveBuffer();

	return result;
}

bool SaveBatteryOrStateAuto(int method, int action, bool silent)
{
	if(method == METHOD_AUTO)
		method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	char filepath[1024];

	if(!MakeFilePath(filepath, action, method, ROMFilename, 0))
		return false;

	return SaveBatteryOrState(filepath, method, action, silent);
}

/****************************************************************************
* Sound
****************************************************************************/

SoundDriver * systemSoundInit()
{
	soundShutdown();
	return new SoundWii();
}

bool systemCanChangeSoundQuality()
{
	return true;
}

void systemOnWriteDataToSoundBuffer(const u16 * finalWave, int length)
{
}

void systemOnSoundShutdown()
{
}

/****************************************************************************
* systemReadJoypads
****************************************************************************/
bool systemReadJoypads()
{
	return true;
}

u32 systemReadJoypad(int which)
{
	if(which == -1) which = 0; // default joypad
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
static int sensorX = 2047;
static int sensorY = 2047;
static int sensorWario = 0x6C0;
static u8 sensorDarkness = 0xE8; // total darkness (including daylight on rainy days)

int systemGetSensorX()
{
	return sensorX;
}

int systemGetSensorY()
{
	return sensorY;
}

int systemGetSensorZ()
{
	return sensorWario;
}

u8 systemGetSensorDarkness()
{
	return sensorDarkness;
}

void systemUpdateSolarSensor()
{
	u8 d, sun;
	switch (SunBars)
	{
		case 0:
			d = 0xE8;
			break;
		case 1:
			d = 0xE0;
			break;
		case 2:
			d = 0xDA;
			break;
		case 3:
			d = 0xD0;
			break;
		case 4:
			d = 0xC8;
			break;
		case 5:
			d = 0xC0;
			break;
		case 6:
			d = 0xB0;
			break;
		case 7:
			d = 0xA0;
			break;
		case 8:
			d = 0x88;
			break;
		case 9:
			d = 0x70;
			break;
		case 10:
			d = 0x50;
			break;
		default:
			d = 0xE8;
			break;
	}
	sun = 0xE8 - d;

	struct tm *newtime;
	time_t long_time;

	// regardless of the weather, there should be no sun at night time!
	time(&long_time); // Get time as long integer.
	newtime = localtime(&long_time); // Convert to local time.
	if (newtime->tm_hour > 21 || newtime->tm_hour < 5)
	{
		sun = 0; // total darkness, 9pm - 5am
	}
	else if (newtime->tm_hour > 20 || newtime->tm_hour < 6)
	{
		sun = sun / 9; // almost total darkness 8pm-9pm, 5am-6am
	}
	else if (newtime->tm_hour > 18 || newtime->tm_hour < 7)
	{
		sun = sun / 2; // half darkness 6pm-8pm, 6am-7am
	}

#ifdef HW_RVL
	// pointing the Gun Del Sol at the ground blocks the sun light,
	// because sometimes you need the shade.
	int chan = 0; // first wiimote
	WPADData *Data = WPAD_Data(chan);
	WPADData data = *Data;
	float f;
	if (data.orient.pitch > 0)
		f = 1.0f - (data.orient.pitch/85.0f);
	else
		f = 1.0f;
	if (f < 0)
		f=0;
	sun *= f;
#endif
	sensorDarkness = 0xE8 - sun;
}

void systemUpdateMotionSensor()
{
#ifdef HW_RVL
	int chan = 0; // first wiimote

	WPADData *Data = WPAD_Data(chan);
	WPADData data = *Data;
	static float OldTiltAngle, OldAvg;
	static bool WasFlat = false;
	float DeltaAngle = 0;

	if (TiltSideways)
	{
		sensorY = 2047+(data.gforce.x*50);
		sensorX = 2047+(data.gforce.y*50);
		TiltAngle = ((-data.orient.pitch) + OldTiltAngle)/2.0f;
		OldTiltAngle = -data.orient.pitch;
	}
	else
	{
		sensorX = 2047-(data.gforce.x*50);
		sensorY = 2047+(data.gforce.y*50);
		TiltAngle = ((data.orient.roll) + OldTiltAngle)/2.0f;
		OldTiltAngle = data.orient.roll;
	}
	DeltaAngle = TiltAngle - OldAvg;
	if (DeltaAngle> 180)
		DeltaAngle -= 360;
	else if (DeltaAngle < -180)
		DeltaAngle += 360;
	OldAvg = TiltAngle;

	if (TiltAngle < 3.0f && TiltAngle> -3.0f)
	{
		WasFlat = true;
		TiltAngle = 0;
	}
	else
	{
		if (WasFlat) TiltAngle = TiltAngle / 2.0f;
		WasFlat = false;
	}

	sensorWario = 0x6C0+DeltaAngle*11;

#endif

	systemUpdateSolarSensor();
}

/****************************************************************************
* systemDrawScreen
****************************************************************************/
static int srcWidth = 0;
static int srcHeight = 0;
static int srcPitch = 0;

void systemDrawScreen()
{
	GX_Render( srcWidth, srcHeight, pix, srcPitch );
}

bool ValidGameId(u32 id)
{
	if (id == 0)
		return false;
	for (int i = 1; i <= 4; i++)
	{
		u8 b = id & 0xFF;
		id = id >> 8;
		if (!(b >= 'A' && b <= 'Z') && !(b >= '0' && b <= '9'))
			return false;
	}
	return true;
}

static void gbApplyPerImagePreferences()
{
	char title[17];
	// Only works for some GB Colour roms
	u8 Colour = gbRom[0x143];
	if (Colour == 0x80 || Colour == 0xC0)
	{
		RomIdCode = gbRom[0x13f] | (gbRom[0x140] << 8) | (gbRom[0x141] << 16)
				| (gbRom[0x142] << 24);
		if (!ValidGameId(RomIdCode))
			RomIdCode = 0;
	}
	else
		RomIdCode = 0;
	// Otherwise we need to make up our own code
	title[15] = '\0';
	title[16] = '\0';
	if (gbRom[0x143] < 0x7F && gbRom[0x143] > 0x20)
		strncpy(title, (const char *) &gbRom[0x134], 16);
	else
		strncpy(title, (const char *) &gbRom[0x134], 15);
	if (RomIdCode == 0)
	{
		if (strcmp(title, "ZELDA") == 0)
			RomIdCode = LINKSAWAKENING;
		else if (strcmp(title, "MORTAL KOMBAT") == 0)
			RomIdCode = MK1;
		else if (strcmp(title, "MORTALKOMBATI&II") == 0)
			RomIdCode = MK12;
		else if (strcmp(title, "MORTAL KOMBAT II") == 0)
			RomIdCode = MK2;
		else if (strcmp(title, "MORTAL KOMBAT 3") == 0)
			RomIdCode = MK3;
		else if (strcmp(title, "MORTAL KOMBAT 4") == 0)
			RomIdCode = MK4;
		else if (strcmp(title, "SUPER MARIOLAND") == 0)
			RomIdCode = MARIOLAND1;
		else if (strcmp(title, "MARIOLAND2") == 0)
			RomIdCode = MARIOLAND2;
		else if (strcmp(title, "METROID2") == 0)
			RomIdCode = METROID2;
		else if (strcmp(title, "MARBLE MADNESS") == 0)
			RomIdCode = MARBLEMADNESS;
		else if (strcmp(title, "TMNT FOOT CLAN") == 0)
			RomIdCode = TMNT1;
		else if (strcmp(title, "TMNT BACK FROM") == 0 || strcmp(title, "TMNT 2") == 0)
			RomIdCode = TMNT2;
		else if (strcmp(title, "TMNT3") == 0)
			RomIdCode = TMNT3;
	}
	// look for matching palettes if a monochrome gameboy game
	// (or if a Super Gameboy game, but the palette will be ignored later in that case)
	int snum = -1;
	if ((Colour != 0x80) && (Colour != 0xC0)) {
		for(int i=1; i < gamePalettesCount; i++)
		{
			if(strcmp(title, gamePalettes[i].gameName)==0)
			{
				snum = i;
				break;
			}
		}
		// match found!
		if(snum >= 0)
		{
			gbSetPalette(gamePalettes[snum].palette);
		} else {
			gbSetPalette(gamePalettes[0].palette);
		}
	}
}

/****************************************************************************
 * ApplyPerImagePreferences
 * Apply game specific settings, originally from vba-over.ini
 ***************************************************************************/

static void ApplyPerImagePreferences()
{
	// look for matching game setting
	int snum = -1;
	RomIdCode = rom[0xac] | (rom[0xad] << 8) | (rom[0xae] << 16) | (rom[0xaf] << 24);

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
	// In most cases this is already handled in GameSettings, but just to make sure:
	switch (rom[0xac])
	{
		case 'F': // Classic NES
			cpuSaveType = 1; // EEPROM
			mirroringEnable = 1;
			break;
		case 'K': // Accelerometers
			cpuSaveType = 4; // EEPROM + sensor
			break;
		case 'R': // WarioWare Twisted style sensors
		case 'V': // Drill Dozer
			rtcEnableWarioRumble(true);
			break;
		case 'U': // Boktai solar sensor and clock
			rtcEnable(true);
			break;
	}
}

void LoadPatch(int method)
{
	int patchsize = 0;
	int patchtype;

	AllocSaveBuffer ();

	char patchpath[3][512];
	memset(patchpath, 0, sizeof(patchpath));
	sprintf(patchpath[0], "%s/%s.ips",browser.dir,ROMFilename);
	sprintf(patchpath[1], "%s/%s.ups",browser.dir,ROMFilename);
	sprintf(patchpath[2], "%s/%s.ppf",browser.dir,ROMFilename);

	for(patchtype=0; patchtype<3; patchtype++)
	{
		patchsize = LoadFile(patchpath[patchtype], method, SILENT);

		if(patchsize)
			break;
	}

	if(patchsize > 0)
	{
		ShowAction("Loading patch...");
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
	gbRom = (u8 *)memalign(32, 1024*1024*4); // allocate 4 MB to GB ROM
	bios = (u8 *)calloc(1,0x100);

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	if(!inSz)
	{
		char filepath[1024];

		if(!MakeFilePath(filepath, FILE_ROM, method))
			return false;

		gbRomSize = LoadFile ((char *)gbRom, filepath, browserList[browser.selIndex].length, method, NOTSILENT);
	}
	else
	{
		switch (method)
		{
			case METHOD_SD:
			case METHOD_USB:
			case METHOD_SMB:
				gbRomSize = LoadSzFile(szpath, (unsigned char *)gbRom);
				break;
			case METHOD_DVD:
				gbRomSize = SzExtractFile(browserList[browser.selIndex].offset, (unsigned char *)gbRom);
				break;
		}
	}

	if(gbRomSize <= 0)
		return false;

	return gbUpdateSizes();
}

bool LoadVBAROM(int method)
{
	cartridgeType = 0;
	bool loaded = false;

	// image type (checks file extension)
	if(utilIsGBAImage(browserList[browser.selIndex].filename))
		cartridgeType = 2;
	else if(utilIsGBImage(browserList[browser.selIndex].filename))
		cartridgeType = 1;
	else if(utilIsZipFile(browserList[browser.selIndex].filename))
	{
		// we need to check the file extension of the first file in the archive
		char * zippedFilename = GetFirstZipFilename (method);

		if(zippedFilename != NULL)
		{
			if(utilIsGBAImage(zippedFilename))
				cartridgeType = 2;
			else if(utilIsGBImage(zippedFilename))
				cartridgeType = 1;
			else {
				ErrorPrompt("Rom must be 1st file in zip, or unzipped!");
				return false;
			}
		}
		else // loading the file failed
		{
			ErrorPrompt("Empty or invalid zip file!");
			return false;
		}
	}

	// leave before we do anything
	if(cartridgeType != 1 && cartridgeType != 2)
	{
		// Not zip gba agb gbc cgb sgb gb mb bin elf or dmg!
		ErrorPrompt("Invalid filename extension! Not a rom?");
		return false;
	}

	srcWidth = 0;
	srcHeight = 0;
	srcPitch = 0;

	VMClose(); // cleanup GBA memory
	gbCleanUp(); // cleanup GB memory

	switch(cartridgeType)
	{
		case 2:
			emulator = GBASystem;
			srcWidth = 240;
			srcHeight = 160;
			loaded = VMCPULoadROM(method);
			srcPitch = 484;
			soundSetSampleRate(44100 / 2);
			cpuSaveType = 0;
			break;

		case 1:
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
			srcPitch = 324;
			soundSetSampleRate(44100);
			break;
	}

	if(!loaded)
	{
		ErrorPrompt("Error loading game!");
		return false;
	}
	else
	{
		// Setup GX
		GX_Render_Init(srcWidth, srcHeight);

		if (cartridgeType == 1)
		{
			gbGetHardwareType();

			// used for the handling of the gb Boot Rom
			//if (gbHardware & 5)
			//gbCPUInit(gbBiosFileName, useBios);

			LoadPatch(method);

			// Apply preferences specific to this game
			gbApplyPerImagePreferences();

			gbSoundReset();
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
			CPUInit(NULL, false);
			LoadPatch(method);
			CPUReset();
		}

		SetAudioRate(cartridgeType);
		soundInit();

		emulating = 1;

		// reset frameskip variables
		lastTime = systemFrameSkip = 0;

		// Start system clock
		start = gettime();

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

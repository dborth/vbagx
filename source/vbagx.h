/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2022
 *
 * vbagx.h
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/
#ifndef _VBAGX_H_
#define _VBAGX_H_

#include <unistd.h>
#include <sys/param.h>

#include "filelist.h"
#include "utils/FreeTypeGX.h"

#define APPNAME 		"Visual Boy Advance GX"
#define APPVERSION 		"2.4.6"
#define APPFOLDER 		"vbagx"
#define PREF_FILE_NAME 	"settings.xml"
#define PAL_FILE_NAME 	"palettes.xml"

#define NOTSILENT 0
#define SILENT 1

const char pathPrefix[9][8] =
{ "", "sd:/", "usb:/", "dvd:/", "smb:/", "carda:/", "cardb:/", "port2:/" };

enum 
{
	DEVICE_AUTO,
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_DVD,
	DEVICE_SMB,
	DEVICE_SD_SLOTA,
	DEVICE_SD_SLOTB,
	DEVICE_SD_PORT2
};

enum 
{
	FILE_SRAM,
	FILE_SNAPSHOT,
	FILE_ROM,
	FILE_BORDER_PNG
};

enum 
{
	LANG_JAPANESE = 0,
	LANG_ENGLISH,
	LANG_GERMAN,
	LANG_FRENCH,
	LANG_SPANISH,
	LANG_ITALIAN,
	LANG_DUTCH,
	LANG_SIMP_CHINESE,
	LANG_TRAD_CHINESE,
	LANG_KOREAN,
	LANG_PORTUGUESE,
	LANG_BRAZILIAN_PORTUGUESE,
	LANG_CATALAN,
	LANG_TURKISH,
	LANG_LENGTH
};

struct SGCSettings
{
	float	gbaZoomHor;    // GBA horizontal zoom amount
	float	gbaZoomVert;   // GBA vertical zoom amount
	float	gbZoomHor;     // GB horizontal zoom amount
	float	gbZoomVert;    // GB vertical zoom amount
	int		gbFixed;
	int		gbaFixed;
	int		AutoLoad;
	int		AutoSave;
	int		LoadMethod;    // For ROMS: Auto, SD, DVD, USB, Network (SMB)
	int		SaveMethod;    // For SRAM, Freeze, Prefs: Auto, SD, USB, SMB
	int		AppendAuto;    // 0 - no, 1 - yes
	int		videomode;     // 0 - automatic, 1 - NTSC (480i), 2 - Progressive (480p), 3 - PAL (50Hz), 4 - PAL (60Hz)
	int		scaling;       // 0 - default, 1 - partial stretch, 2 - stretch to fit, 3 - widescreen correction
	int		render;		   // 0 - original, 1 - filtered, 2 - unfiltered
	int		xshift;		   // video output shift
	int		yshift;
	int		colorize;      // colorize Mono Gameboy games
	int		gbaFrameskip;  // turn on auto-frameskip for GBA games
	int		WiiControls;   // Match Wii Game
	int		WiimoteOrientation;
	int		ExitAction;
	int		MusicVolume;
	int		SFXVolume;
	int		Rumble;
	int 	language;
	int		PreviewImage;
	int		TurboModeEnabled; // 0 - disabled, 1 - enabled
	int		AutoloadGame;
	
	int		OffsetMinutesUTC; // Used for clock on MBC3 and TAMA5
	int 	GBHardware;    // Mapped to gbEmulatorType in VBA
	int 	SGBBorder;
	int		BasicPalette;	// 0 - Green   1 - Monochrome
	
	char	LoadFolder[MAXPATHLEN];  // Path to game files
	char	LastFileLoaded[MAXPATHLEN]; //Last file loaded filename
	char	SaveFolder[MAXPATHLEN];  // Path to save files
	char	ScreenshotsFolder[MAXPATHLEN]; //Path to screenshots files
	char	CoverFolder[MAXPATHLEN]; 	//Path to cover files
	char	ArtworkFolder[MAXPATHLEN]; 	//Path to artwork files
	char	BorderFolder[MAXPATHLEN];  // Path to Super Game Boy border files

	char	smbip[80];
	char	smbuser[20];
	char	smbpwd[20];
	char	smbshare[20];
};

void ExitApp();
void ShutdownWii();
bool SupportedIOS(u32 ios);
bool SaneIOS(u32 ios);
extern struct SGCSettings GCSettings;
extern int ScreenshotRequested;
extern int ConfigRequested;
extern int ShutdownRequested;
extern int ExitRequested;
extern char appPath[];

extern FreeTypeGX *fontSystem[];
extern bool isWiiVC;
static inline bool IsWiiU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) || isWiiVC);
}
static inline bool IsWiiUFastCPU(void)
{
	return ((*(vu16*)0xCD8005A0 == 0xCAFE) && ((*(vu32*)0xCD8005B0 & 0x20) == 0));
}

#endif

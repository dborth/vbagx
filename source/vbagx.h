/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
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
#define APPVERSION 		"2.5.1"
#define APPFOLDER 		"vbagx"
#define PREF_FILE_NAME 	"settings.xml"
#define PAL_FILE_NAME 	"palettes.xml"

#define NOTSILENT 0
#define SILENT 1

const char pathPrefix[10][11] =
{ "", "sd:/", "usb:/", "dvd:/", "smb:/", "carda:/", "cardb:/", "port2:/", "gcloader:/" };

enum 
{
	DEVICE_AUTO = 0,
	DEVICE_SD,
	DEVICE_USB,
	DEVICE_DVD,
	DEVICE_SMB,
	DEVICE_SD_SLOTA,
	DEVICE_SD_SLOTB,
	DEVICE_SD_PORT2,
	DEVICE_SD_GCLOADER,
	DEVICE_LENGTH
};

enum {
    SAVEFOLDER_SAVES = 0,
    SAVEFOLDER_LENGTH
};

enum {
    LOADFOLDER_ROMS = 0,
    LOADFOLDER_SCREENSHOTS,
    LOADFOLDER_COVERS,
    LOADFOLDER_ARTWORK,
	LOADFOLDER_BORDERS,
    LOADFOLDER_LENGTH
};

typedef struct {
    int id;
    const char *name;
} FolderDef;

const FolderDef saveFolder[] = {
    { SAVEFOLDER_SAVES,  "saves" }
};

const FolderDef loadFolder[] = {
    { LOADFOLDER_ROMS,        "roms" },
    { LOADFOLDER_SCREENSHOTS, "screenshots" },
    { LOADFOLDER_COVERS,      "covers" },
    { LOADFOLDER_ARTWORK,     "artwork" },
	{ LOADFOLDER_BORDERS,     "borders" }
};

enum 
{
	FILE_SRAM,
	FILE_SNAPSHOT,
	FILE_ROM,
	FILE_BORDER_PNG
};

enum {
	PREVIEWIMAGE_SCREENSHOT = 0,
	PREVIEWIMAGE_COVER,
	PREVIEWIMAGE_ARTWORK,
	PREVIEWIMAGE_LENGTH
};

enum {
	RENDER_FILTERED = 1,
	RENDER_UNFILTERED,
	RENDER_FILTERED_SOFT,
	RENDER_FILTERED_SHARP,
	RENDER_LENGTH
};

enum {
	VIDEOMODE_AUTO = 0,
	VIDEOMODE_NTSC,
	VIDEOMODE_PROGRESSIVE,
	VIDEOMODE_PAL,
	VIDEOMODE_EURGB,
	VIDEOMODE_240P,
	VIDEOMODE_EURGB_240P,
	VIDEOMODE_LENGTH
};

enum {
	WIIMOTEORIENTATION_VERTICAL = 0,
	WIIMOTEORIENTATION_HORIZONTAL,
	WIIMOTEORIENTATION_LENGTH
};

enum {
	SCALING_MAINTAIN_ASPECT = 0,
	SCALING_PARTIAL_STRETCH,
	SCALING_STRETCH_TO_FIT,
	SCALING_WIDESCREEN_CORRECTION,
	SCALING_LENGTH
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
	LANG_SWEDISH,
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
extern struct SGCSettings GCSettings;
extern int ScreenshotRequested;
extern int ConfigRequested;
extern char appPath[];

extern FreeTypeGX *fontSystem[];

#endif

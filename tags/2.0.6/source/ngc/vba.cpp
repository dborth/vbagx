/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vba.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <wiiuse/wpad.h>

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "FreeTypeGX.h"

#include "gba/Sound.h"

#include "vba.h"
#include "vbasupport.h"
#include "preferences.h"
#include "audio.h"
#include "dvd.h"
#include "networkop.h"
#include "filebrowser.h"
#include "fileop.h"
#include "menu.h"
#include "input.h"
#include "video.h"
#include "vbaconfig.h"
#include "gamesettings.h"
#include "wiiusbsupport.h"

extern int emulating;
void StopColorizing();
void gbSetPalette(u32 RRGGBB[]);
int ScreenshotRequested = 0;
int ConfigRequested = 0;
int ShutdownRequested = 0;
int ResetRequested = 0;
int ExitRequested = 0;
char appPath[1024];
int appLoadMethod = METHOD_AUTO;

extern FILE *out;

/****************************************************************************
 * Shutdown / Reboot / Exit
 ***************************************************************************/

static void ExitCleanup()
{
#ifdef HW_RVL
	ShutoffRumble();
#endif
	ShutdownAudio();
	StopGX();
	if (out) fclose(out);

	HaltDeviceThread();
	UnmountAllFAT();

#ifdef HW_RVL
	DI_Close();
#endif
}

#ifdef HW_DOL
	#define PSOSDLOADID 0x7c6000a6
	int *psoid = (int *) 0x80001800;
	void (*PSOReload) () = (void (*)()) 0x80001800;
#endif

void ExitApp()
{
	SavePrefs(SILENT);

	if (ROMLoaded && !ConfigRequested && GCSettings.AutoSave == 1)
		SaveBatteryOrStateAuto(GCSettings.SaveMethod, FILE_SRAM, SILENT);

	ExitCleanup();

	if(ShutdownRequested)
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);

	#ifdef HW_RVL
	if(GCSettings.ExitAction == 0) // Auto
	{
		char * sig = (char *)0x80001804;
		if(
			sig[0] == 'S' &&
			sig[1] == 'T' &&
			sig[2] == 'U' &&
			sig[3] == 'B' &&
			sig[4] == 'H' &&
			sig[5] == 'A' &&
			sig[6] == 'X' &&
			sig[7] == 'X')
			GCSettings.ExitAction = 3; // Exit to HBC
		else
			GCSettings.ExitAction = 1; // HBC not found
	}
	#endif

	if(GCSettings.ExitAction == 1) // Exit to Menu
	{
		#ifdef HW_RVL
			SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
		#else
			#define SOFTRESET_ADR ((volatile u32*)0xCC003024)
			*SOFTRESET_ADR = 0x00000000;
		#endif
	}
	else if(GCSettings.ExitAction == 2) // Shutdown Wii
	{
		SYS_ResetSystem(SYS_POWEROFF, 0, 0);
	}
	else // Exit to Loader
	{
		#ifdef HW_RVL
			exit(0);
		#else
			if (psoid[0] == PSOSDLOADID)
				PSOReload();
		#endif
	}
}

#ifdef HW_RVL
void ShutdownCB()
{
	ShutdownRequested = 1;
}
void ResetCB()
{
	ResetRequested = 1;
}
#endif

#ifdef HW_DOL
/****************************************************************************
 * ipl_set_config
 * lowlevel Qoob Modchip disable
 ***************************************************************************/

static void ipl_set_config(unsigned char c)
{
	volatile unsigned long* exi = (volatile unsigned long*)0xCC006800;
	unsigned long val,addr;
	addr=0xc0000000;
	val = c << 24;
	exi[0] = ((((exi[0]) & 0x405) | 256) | 48);	//select IPL
	//write addr of IPL
	exi[0 * 5 + 4] = addr;
	exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
	while (exi[0 * 5 + 3] & 1);
	//write the ipl we want to send
	exi[0 * 5 + 4] = val;
	exi[0 * 5 + 3] = ((4 - 1) << 4) | (1 << 2) | 1;
	while (exi[0 * 5 + 3] & 1);

	exi[0] &= 0x405;	//deselect IPL
}
#endif

static void CreateAppPath(char origpath[])
{
#ifdef HW_DOL
	sprintf(appPath, GCSettings.SaveFolder);
#else
	char path[1024];
	strncpy(path, origpath, 1024); // make a copy so we don't mess up original

	char * loc;
	int pos = -1;

	if(strncmp(path, "sd:/", 5) == 0 || strncmp(path, "fat:/", 5) == 0)
		appLoadMethod = METHOD_SD;
	else if(strncmp(path, "usb:/", 5) == 0)
		appLoadMethod = METHOD_USB;

	loc = strrchr(path,'/');
	if (loc != NULL)
		*loc = 0; // strip file name

	loc = strchr(path,'/'); // looking for first / (after sd: or usb:)
	if (loc != NULL)
		pos = loc - path + 1;

	if(pos >= 0 && pos < 1024)
		sprintf(appPath, &(path[pos]));
#endif
}

/****************************************************************************
* main
*
* Program entry
****************************************************************************/
int main(int argc, char *argv[])
{
	#ifdef HW_DOL
	ipl_set_config(6); // disable Qoob modchip
	#endif

	#ifdef HW_RVL
	DI_Close(); // fixes some black screen issues
	DI_Init();	// first
	#endif

	InitDeviceThread();
	VIDEO_Init();
	PAD_Init();

	#ifdef HW_RVL
	WPAD_Init();
	#endif

	InitializeVideo();
	ResetVideo_Menu (); // change to menu video mode

	#ifdef HW_RVL
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL,screenwidth,screenheight);

	// Wii Power/Reset buttons
	WPAD_SetPowerButtonCallback((WPADShutdownCallback)ShutdownCB);
	SYS_SetPowerCallback(ShutdownCB);
	SYS_SetResetCallback(ResetCB);
	#endif

	MountAllFAT(); // Initialize libFAT for SD and USB

	// Initialize DVD subsystem (GameCube only)
	#ifdef HW_DOL
	DVD_Init ();
	#endif

	SetDVDDriveType(); // Check if DVD drive belongs to a Wii
	InitialiseSound();
	InitialisePalette();
	DefaultSettings (); // Set defaults

	// Initialize font system
	InitFreeType((u8*)font_ttf, font_ttf_size);

	InitGUIThreads();

	// store path app was loaded from
	sprintf(appPath, "vbagx");
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);

	StartWiiKeyboardMouse();

	while(1) // main loop
	{
		// go back to checking if devices were inserted/removed
		// since we're entering the menu
		ResumeDeviceThread();

		SwitchAudioMode(1);

		if(!ROMLoaded)
			MainMenu(MENU_GAMESELECTION);
		else
			MainMenu(MENU_GAME);

		ConfigRequested = 0;
		ScreenshotRequested = 0;
		SwitchAudioMode(0);

		// stop checking if devices were removed/inserted
		// since we're starting emulation again
		HaltDeviceThread();

		ResetVideo_Emu();

		// GB colorizing - set palette
		if(IsGameboyGame())
		{
			if(GCSettings.colorize && strcmp(RomTitle, "MEGAMAN") != 0)
				gbSetPalette(CurrentPalette.palette);
			else
				StopColorizing();
		}

		while (emulating) // emulation loop
		{
			emulator.emuMain(emulator.emuCount);

			if(ResetRequested)
			{
				emulator.emuReset(); // reset game
				ResetRequested = 0;
			}
			if(ConfigRequested)
			{
				ResetVideo_Menu();
				break; // leave emulation loop
			}
			#ifdef HW_RVL
			if(ShutdownRequested)
				ExitApp();
			#endif
		} // emulation loop
	} // main loop
	return 0;
}

/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
 *
 * vbagx.cpp
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/

#include "vbagx.h"
#include "system.h"
#include "vbasupport.h"
#include "preferences.h"
#include "audio.h"
#include "filebrowser.h"
#include "fileop.h"
#include "menu.h"
#include "input.h"
#include "video.h"
#include "gamesettings.h"
#include "mem2.h"
#include "videofilters.h"

#include "vba/gba/Globals.h"
#include "vba/gba/Sound.h"
#include "vba/gba/JIT.h"

extern int emulating;
void StopColorizing();
void gbSetPalette(u32 RRGGBB[]);
int ScreenshotRequested = 0;
int ConfigRequested = 0;
char appPath[1024] = { 0 };
static bool autoboot = false;

/****************************************************************************
* main
*
* Program entry
****************************************************************************/
int main(int argc, char *argv[])
{
	DefaultSettings (); // Set defaults
	SystemInit();
	ResetVideo_Menu (); // change to menu video mode
	
	#ifdef HW_RVL
	// store path app was loaded from
	if(argc > 0 && argv[0] != NULL)
		CreateAppPath(argv[0]);
	#endif

	InitialisePalette();

#ifdef HW_RVL
	savebuffer = (unsigned char *)mem2_malloc(SAVEBUFFERSIZE);
	browserList = (BROWSERENTRY *)mem2_malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
	rom = (u8 *)mem2_malloc(1024*1024*32); // allocate 32 MB to GBA ROM
#else
	savebuffer = (unsigned char *)malloc(SAVEBUFFERSIZE);
	browserList = (BROWSERENTRY *)malloc(sizeof(BROWSERENTRY)*MAX_BROWSER_SIZE);
#endif
	InitGUIThreads();

#ifdef HW_RVL
	if(argc > 2 && argv[1] != NULL && argv[2] != NULL) {
		LoadPrefs();
		if(strncmp(argv[1], "sd", 2) == 0)
		{
			GCSettings.SaveMethod = DEVICE_SD;
			GCSettings.LoadMethod = DEVICE_SD;
		}
		else if(strncmp(argv[1], "usb", 3) == 0)
		{
			GCSettings.SaveMethod = DEVICE_USB;
			GCSettings.LoadMethod = DEVICE_USB;
		}
		SavePrefs(SILENT);

		GCSettings.AutoloadGame = AutoloadGame(argv[1], argv[2]);
		autoboot = GCSettings.AutoloadGame;
	}
#endif

	while(1) // main loop
	{
		if(!autoboot) {
			// go back to checking if devices were inserted/removed
			// since we're entering the menu
			ResumeDeviceThread();

			SwitchAudioMode(1);

			if(!ROMLoaded)
				MainMenu(MENU_GAMESELECTION);
			else
				MainMenu(MENU_GAME);
		}

		autoboot = false;
		ConfigRequested = 0;
		ScreenshotRequested = 0;

		SwitchAudioMode(0);

		SelectFilterMethod(GCSettings.FilterMethod); // Initialize / Re-evaluate active filter

		// stop checking if devices were removed/inserted
		// since we're starting emulation again
		HaltDeviceThread();
		ResetTiltAndCursor();
		ResetVideo_Emu();

		// GB colorizing - set palette
		if(IsGameboyGame())
		{
			if(GCSettings.colorize && strcmp(RomTitle, "MEGAMAN") != 0)
				gbSetPalette(CurrentPalette.palette);
			else
				StopColorizing();
		}
		#ifndef NO_JIT_COMPILER
		JIT_RESET_LOGS();
		#endif

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

		#ifndef NO_JIT_COMPILER
		JIT_OUTPUT_LOGS();
		#endif
	} // main loop
	return 0;
}

void ExitApp()
{
	SavePrefs(SILENT);

	if (ROMLoaded && !ConfigRequested && GCSettings.AutoSave == AUTOSAVE_SRAM)
		SaveBatteryOrStateAuto(FILE_SRAM, SILENT);

	SystemExit(GCSettings.ExitAction, autoboot);
}

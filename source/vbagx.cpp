/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
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
#include "memmanager.h"
#include "drivers/ogc/videofilters.h"
#include "drivers/ogc/vm/vmpager.h"

#include "vba/gba/Globals.h"
#include "vba/gba/Sound.h"
#include "vba/gba/JIT.h"

extern int emulating;
void StopColorizing();
void gbSetPalette(u32 RRGGBB[]);
bool MenuRequested = false;
char appPath[1024] = { 0 };
static bool autoboot = false;

/****************************************************************************
* main
*
* Program entry
****************************************************************************/
int main(int argc, char *argv[])
{
	DefaultSettings();
	SystemInit();
	ApplySettings();
	ResetVideo_Menu(); // change to menu video mode
	
	#ifdef HW_RVL
	// store path app was loaded from
	if(argc > 0 && argv[0] != nullptr)
		CreateAppPath(argv[0]);
	#endif

	InitGUI();

#ifdef HW_RVL
	if(argc > 2 && argv[1] != nullptr && argv[2] != nullptr) {
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
		SavePrefs();

		GCSettings.AutoloadGame = AutoloadGame(argv[1], argv[2]);
		autoboot = GCSettings.AutoloadGame;
	}
#endif

	while (!ExitRequested && !ShutdownRequested) // main loop
	{
		if(!autoboot) {
			// go back to checking if devices were inserted/removed
			// since we're entering the menu
			ResumeDeviceCheckingThread();

			SwitchMemoryModeMenu();
			SwitchAudioMode(1);

			if(!ROMLoaded)
				MainMenu(MENU_GAMESELECTION);
			else
				MainMenu(MENU_GAME);
		}

		if(ExitRequested || ShutdownRequested) {
			break;
		}

		autoboot = false;
		MenuRequested = false;
		InitGameDimensionsAndBorder();
		SwitchMemoryModeGame();
		SwitchAudioMode(0);
		SelectFilterMethod(GCSettings.videoUpscalingFilter); // Initialize / Re-evaluate active filter

		// stop checking if devices were removed/inserted
		// since we're starting emulation again
		HaltDeviceCheckingThread();
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
		DEBUG_RESET_LOGS();

		systemResetPacer();
		while (emulating && !MenuRequested && !ExitRequested && !ShutdownRequested) // emulation loop
		{
			emulator.emuMain(emulator.emuCount);

			if(ResetRequested)
			{
				emulator.emuReset(); // reset game
				ResetRequested = 0;
			}
			if(MenuRequested)
			{
				MenuRequested = false;
				uint8_t *tempBuffer = (uint8_t *)malloc(TEXTUREMEM_SIZE); // this one needs to stay malloc because we're switching modes!
				memcpy(tempBuffer, texturemem, TEXTUREMEM_SIZE);
				SwitchMemoryModeMenu();
				TakeScreenshot(tempBuffer);
				free(tempBuffer);
				ResetVideo_Menu();

				#ifdef HW_DOL
				VMPager_Pause();
				#endif
				break; // leave emulation loop
			}
		} // emulation loop

		DEBUG_OUTPUT_LOGS();
	} // main loop
	ExitApp();
}

void ExitApp()
{
	SwitchMemoryModeMenu();
	SavePrefs();

	if (ROMLoaded && !MenuRequested && GCSettings.AutoSave == AUTOSAVE_SRAM)
		SaveBatteryOrStateAuto(FILE_SRAM, SILENT);

	SystemExit(GCSettings.ExitAction, autoboot);
}

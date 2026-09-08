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
#include "vbasupport.h"
#include "preferences.h"
#include "filebrowser.h"
#include "fileop.h"
#include "menu.h"
#include "input.h"
#include "video.h"
#include "gamesettings.h"
#include "memmanager.h"
#include "font_ttf.h"
#include "libgui/Gui.h"
#include "drivers/Platform.h"
#include "drivers/Thread.h"
#include "drivers/EmulatorVideoDriver.h"

#include "vba/gba/Globals.h"
#include "vba/gba/Sound.h"
#include "vba/gba/JIT.h"

#if defined(HW_RVL) || defined(HW_DOL)
#include "drivers/ogc/videofilters.h"
#endif

#ifdef HW_DOL
#include "drivers/ogc/vm/vmpager.h"
#endif

#ifdef HW_DOL
#include "drivers/ogc/GameCubePlatform.h"
static GameCubePlatform platformInstance;
#else
#include "drivers/ogc/WiiPlatform.h"
static WiiPlatform platformInstance;
#endif
Platform* platform = &platformInstance;

extern int emulating;
void StopColorizing();
void gbSetPalette(u32 RRGGBB[]);

AppRequest appRequest = AppRequest::NONE;
char appPath[1024] = { 0 };
static bool autoboot = false;

/****************************************************************************
* main
*
* Program entry
****************************************************************************/
int main(int argc, char *argv[])
{
	InitMemManager();
	platform->init(640, 480);
	SwitchMemoryModeMenu();
	platform->getVideo()->getEmulatorVideo()->initFPSFontData();

	InitFileOpThreads();
	MountAllFAT();

	fontSystem = new GuiTextRenderer(font_ttf, font_ttf_size, platform->getVideo()->getGlyphRenderer());
	textTranslator = new GuiTextTranslator();
	textTranslator->loadLanguage(en_lang, en_lang_size);

	DefaultSettings();
	ApplySettings();
	platform->getVideo()->startMenuVideo();
	
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

	while (appRequest != AppRequest::EXIT && platform->getSystemEvent() != SystemEvent::ShutdownRequested) // main loop
	{
		if(!autoboot) {
			// go back to checking if devices were inserted/removed
			// since we're entering the menu
			ResumeDeviceCheckingThread();

			SwitchMemoryModeMenu();
			platform->getAudio()->startMenuAudio();

			if(!ROMLoaded)
				MainMenu(MENU_GAMESELECTION);
			else
				MainMenu(MENU_GAME);
		}

		if(appRequest == AppRequest::EXIT || platform->getSystemEvent() == SystemEvent::ShutdownRequested) {
			break;
		}

		autoboot = false;
		appRequest = AppRequest::NONE;
		InitGameDimensionsAndBorder();
		SwitchMemoryModeGame();
		platform->getAudio()->startEmulatorAudio();
#if defined(HW_RVL) || defined(HW_DOL)
		SelectFilterMethod(GCSettings.videoUpscalingFilter); // Initialize / Re-evaluate active filter
#endif

		// stop checking if devices were removed/inserted
		// since we're starting emulation again
		HaltDeviceCheckingThread();
		ResetTiltAndCursor();
		platform->getVideo()->getEmulatorVideo()->resetVideo();

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
		while (emulating && appRequest == AppRequest::NONE) // emulation loop
		{
			SystemEvent event = platform->getSystemEvent(); // poll exactly once per iteration - see WiiPlatform::getSystemEvent()
			if(event == SystemEvent::ShutdownRequested)
				break;

			emulator.emuMain(emulator.emuCount);

			if(event == SystemEvent::ResetRequested)
			{
				emulator.emuReset(); // reset game
			}
			if(appRequest == AppRequest::MENU)
			{
				appRequest = AppRequest::NONE;
				uint8_t *tempBuffer = (uint8_t *)malloc(TEXTUREMEM_SIZE); // this one needs to stay malloc because we're switching modes!
				memcpy(tempBuffer, texturemem, TEXTUREMEM_SIZE);
				SwitchMemoryModeMenu();
				TakeScreenshot(tempBuffer);
				free(tempBuffer);
				platform->getVideo()->startMenuVideo();

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

	if (ROMLoaded && appRequest != AppRequest::MENU && GCSettings.AutoSave == AUTOSAVE_SRAM)
		SaveBatteryOrStateAuto(FILE_SRAM, SILENT);

	HaltDeviceCheckingThread();

	// Generic safety net: stop and join every Thread still outstanding
	// (device/parse/worker) before any driver it might touch gets torn
	// down inside requestExit()/shutdown().
	Thread::JoinAll();

	platform->requestExit(GCSettings.ExitAction, autoboot);
}

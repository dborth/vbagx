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
#include <sdcard/card_cmn.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/card_io.h>
#include <fat.h>

#ifdef WII_DVD
extern "C" {
#include <di/di.h>
}
#endif

#include "vba.h"
#include "vbasupport.h"
#include "preferences.h"
#include "audio.h"
#include "dvd.h"
#include "smbop.h"
#include "menu.h"
#include "menudraw.h"
#include "input.h"
#include "video.h"
#include "vbaconfig.h"

extern bool ROMLoaded;
extern int emulating;
int ConfigRequested = 0;

/****************************************************************************
 * ipl_set_config
 * lowlevel Qoob Modchip disable
 ***************************************************************************/

void ipl_set_config(unsigned char c)
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

/****************************************************************************
* main
*
* Program entry
****************************************************************************/
int main()
{
	#ifdef HW_DOL
	ipl_set_config(6); // disable Qoob modchip
	#endif

	#ifdef WII_DVD
	DI_Init();	// first
	#endif

	// Initialise controllers
	#ifdef HW_RVL
	WPAD_Init();
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
	#endif
	PAD_Init ();

	int selectedMenu = -1;

	InitialiseVideo();

	// Initialise freetype font system
	if (FT_Init ())
	{
		printf ("Cannot initialise font subsystem!\n");
		while (1);
	}

	// Initialize libFAT for SD and USB
	fatInit (8, false);

	// Initialize DVD subsystem (GameCube only)
	#ifdef HW_DOL
	DVD_Init ();
	#endif

	// Check if DVD drive belongs to a Wii
	SetDVDDriveType();

	InitialiseSound();

	InitialisePalette();

	// Set defaults
	DefaultSettings ();

	// Load preferences
	if(!LoadPrefs())
	{
		WaitPrompt((char*) "Preferences reset - check settings!");
		selectedMenu = 2; // change to preferences menu
	}

	while(1) // main loop
	{
		ResetVideo_Menu (); // change to menu video mode
		MainMenu(selectedMenu);
		selectedMenu = 3; // return to game menu from now on

		while (emulating) // emulation loop
		{
			emulator.emuMain(emulator.emuCount);

			if(ConfigRequested)
			{
				if (GCSettings.AutoSave == 1)
				{
					SaveBatteryOrState(GCSettings.SaveMethod, 0, SILENT); // save battery
				}
				else if (GCSettings.AutoSave == 2)
				{
					SaveBatteryOrState(GCSettings.SaveMethod, 1, SILENT); // save state
				}
				else if(GCSettings.AutoSave == 3)
				{
					SaveBatteryOrState(GCSettings.SaveMethod, 0, SILENT); // save battery
					SaveBatteryOrState(GCSettings.SaveMethod, 1, SILENT); // save state
				}
				ConfigRequested = 0;
				break;
			}
		}
	}
}

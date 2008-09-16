/****************************************************************************
* VisualBoyAdvance 1.7.2
* Nintendo GameCube Wrapper
****************************************************************************/


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

#include "vbasupport.h"
#include "audio.h"
#include "dvd.h"
#include "smbop.h"
#include "menu.h"
#include "menudraw.h"
#include "input.h"
#include "video.h"
#include "vbaconfig.h"

extern int ROMSize;
extern int emulating;


/****************************************************************************
* main
*
* Program entry
****************************************************************************/
int main()
{
#ifdef WII_DVD
	DI_Init();	// first
#endif

	int selectedMenu = -1;

	PAD_Init ();			/*** Initialise pads for input ***/
	#ifdef HW_RVL
	WPAD_Init();
	// read wiimote accelerometer and IR data
	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL,640,480);
	#endif

	InitialiseVideo();

	// Initialise freetype font system
	if (FT_Init ())
	{
		printf ("Cannot initialise font subsystem!\n");
		while (1);
	}

	// Initialize libFAT for SD and USB
	fatInitDefault();

	// Initialize DVD subsystem (GameCube only)
	#ifndef HW_RVL
	DVD_Init ();
	#endif

	// Check if DVD drive belongs to a Wii
	SetDVDDriveType();

	InitialiseSound();

	InitialisePalette();

	// Set defaults
	DefaultSettings ();

	while (ROMSize == 0)
	{
		mainmenu (selectedMenu);
	}

	//Main loop
	while(1)
	{
		while (emulating)
		{
			emulator.emuMain(emulator.emuCount);
		}
	}

  // Never leaving here
  while(1);

  return 0;
}


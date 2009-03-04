/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * menu.cpp
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#ifdef HW_RVL
extern "C" {
#include <di/di.h>
}
#endif

#include "vba.h"
#include "vbasupport.h"
#include "audio.h"
#include "video.h"
#include "filesel.h"
#include "gcunzip.h"
#include "networkop.h"
#include "memcardop.h"
#include "fileop.h"
#include "dvd.h"
#include "preferences.h"
#include "button_mapping.h"
#include "menudraw.h"
#include "input.h"
#include "vbaconfig.h"
#include "wiiusbsupport.h"

extern "C"
{
#include "tbtime.h"
}

extern int menu;
extern bool ROMLoaded;

/****************************************************************************
 * Load Manager
 ***************************************************************************/

static int
LoadManager ()
{
	int loadROM = OpenROM(GCSettings.LoadMethod);

	/***
	* check for autoload battery / state
	***/
	if ( loadROM == 1 ) // if ROM was loaded, load the battery / state
	{
		if (GCSettings.AutoLoad == 1)
			LoadBatteryOrState(GCSettings.SaveMethod, FILE_SRAM, SILENT); // load battery
		else if (GCSettings.AutoLoad == 2)
			LoadBatteryOrState(GCSettings.SaveMethod, FILE_SNAPSHOT, SILENT); // load state
	}

	return loadROM;
}

/****************************************************************************
 * Preferences Menu
 ***************************************************************************/

static void
PreferencesMenu ()
{
	int prefmenuCount = 12;
	char prefmenu[][50] = {

		"Load Method",
		"Load Folder",
		"Save Method",
		"Save Folder",

		"Auto Load",
		"Auto Save",
		"Verify MC Saves",
		"Enable Zooming",
		"Video Rendering",
		"Video Scaling",

		"Reset Preferences",
		"Back to Main Menu"
	};

	int ret = 0;
	int quit = 0;
	int oldmenu = menu;
	menu = 0;
	while (quit == 0)
	{
		// some load/save methods are not implemented - here's where we skip them
		// they need to be skipped in the order they were enumerated in vba.h

		// no USB ports on GameCube
		#ifdef HW_DOL
		if(GCSettings.LoadMethod == METHOD_USB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_USB)
			GCSettings.SaveMethod++;
		#endif

		// saving to DVD is impossible
		if(GCSettings.SaveMethod == METHOD_DVD)
			GCSettings.SaveMethod++;

		// disable DVD in GC mode (not implemented)
		#ifdef HW_DOL
		if(GCSettings.LoadMethod == METHOD_DVD)
			GCSettings.LoadMethod++;
		#endif

		// disable SMB in GC mode (stalls out)
		#ifdef HW_DOL
		if(GCSettings.LoadMethod == METHOD_SMB)
			GCSettings.LoadMethod++;
		if(GCSettings.SaveMethod == METHOD_SMB)
			GCSettings.SaveMethod++;
		#endif

		// disable MC saving in Wii mode - does not work for some reason!
		#ifdef HW_RVL
		if(GCSettings.SaveMethod == METHOD_MC_SLOTA)
			GCSettings.SaveMethod++;
		if(GCSettings.SaveMethod == METHOD_MC_SLOTB)
			GCSettings.SaveMethod++;
		prefmenu[6][0] = 0;
		#else
		sprintf (prefmenu[6], "Verify MC Saves %s", GCSettings.VerifySaves == true ? " ON" : "OFF");
		#endif

		// disable Widescreen correction in Wii mode - determined automatically
		#ifdef HW_RVL
		if(GCSettings.scaling == 3)
			GCSettings.scaling = 0;
		#endif

		// correct load/save methods out of bounds
		if(GCSettings.LoadMethod > 4)
			GCSettings.LoadMethod = 0;
		if(GCSettings.SaveMethod > 6)
			GCSettings.SaveMethod = 0;

		if (GCSettings.LoadMethod == METHOD_AUTO) sprintf (prefmenu[0],"Load Method AUTO");
		else if (GCSettings.LoadMethod == METHOD_SD) sprintf (prefmenu[0],"Load Method SD");
		else if (GCSettings.LoadMethod == METHOD_USB) sprintf (prefmenu[0],"Load Method USB");
		else if (GCSettings.LoadMethod == METHOD_DVD) sprintf (prefmenu[0],"Load Method DVD");
		else if (GCSettings.LoadMethod == METHOD_SMB) sprintf (prefmenu[0],"Load Method Network");

		sprintf (prefmenu[1], "Load Folder %s",	GCSettings.LoadFolder);

		if (GCSettings.SaveMethod == METHOD_AUTO) sprintf (prefmenu[2],"Save Method AUTO");
		else if (GCSettings.SaveMethod == METHOD_SD) sprintf (prefmenu[2],"Save Method SD");
		else if (GCSettings.SaveMethod == METHOD_USB) sprintf (prefmenu[2],"Save Method USB");
		else if (GCSettings.SaveMethod == METHOD_SMB) sprintf (prefmenu[2],"Save Method Network");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTA) sprintf (prefmenu[2],"Save Method MC Slot A");
		else if (GCSettings.SaveMethod == METHOD_MC_SLOTB) sprintf (prefmenu[2],"Save Method MC Slot B");

		sprintf (prefmenu[3], "Save Folder %s",	GCSettings.SaveFolder);

		// disable changing load/save directories for now
		prefmenu[1][0] = '\0';
		prefmenu[3][0] = '\0';

		if (GCSettings.AutoLoad == 0) sprintf (prefmenu[4],"Auto Load OFF");
		else if (GCSettings.AutoLoad == 1) sprintf (prefmenu[4],"Auto Load SRAM");
		else if (GCSettings.AutoLoad == 2) sprintf (prefmenu[4],"Auto Load SNAPSHOT");

		if (GCSettings.AutoSave == 0) sprintf (prefmenu[5],"Auto Save OFF");
		else if (GCSettings.AutoSave == 1) sprintf (prefmenu[5],"Auto Save SRAM");
		else if (GCSettings.AutoSave == 2) sprintf (prefmenu[5],"Auto Save SNAPSHOT");
		else if (GCSettings.AutoSave == 3) sprintf (prefmenu[5],"Auto Save BOTH");

		sprintf (prefmenu[7], "Enable Zooming %s",
			GCSettings.Zoom == true ? " ON" : "OFF");

		// original mode not implemented
		if(GCSettings.render == 0)
			GCSettings.render++;

		if (GCSettings.render == 0)
			sprintf (prefmenu[8], "Video Rendering Original");
		if (GCSettings.render == 1)
			sprintf (prefmenu[8], "Video Rendering Filtered");
		if (GCSettings.render == 2)
			sprintf (prefmenu[8], "Video Rendering Unfiltered");

		if (GCSettings.scaling == 0)
			sprintf (prefmenu[9], "Video Scaling Maintain Aspect Ratio");
		else if (GCSettings.scaling == 1)
			sprintf (prefmenu[9], "Video Scaling Partial Stretch");
		else if (GCSettings.scaling == 2)
			sprintf (prefmenu[9], "Video Scaling Stretch to Fit");
		else if (GCSettings.scaling == 3)
			sprintf (prefmenu[9], "Video Scaling 16:9 Correction");

		ret = RunMenu (prefmenu, prefmenuCount, "Preferences", 16);

		switch (ret)
		{
			case 0:
				GCSettings.LoadMethod ++;
				break;

			case 1:
				break;

			case 2:
				GCSettings.SaveMethod ++;
				break;

			case 3:
				break;

			case 4:
				GCSettings.AutoLoad ++;
				if (GCSettings.AutoLoad > 2)
					GCSettings.AutoLoad = 0;
				break;

			case 5:
				GCSettings.AutoSave ++;
				if (GCSettings.AutoSave > 3)
					GCSettings.AutoSave = 0;
				break;

			case 6:
				GCSettings.VerifySaves ^= 1;
				break;

			case 7:
				GCSettings.Zoom ^= 1;
				break;

			case 8:
				GCSettings.render++;
				if (GCSettings.render > 2)
					GCSettings.render = 0;
				// reset zoom
				zoom_reset ();
				break;

			case 9:
				GCSettings.scaling++;
				if (GCSettings.scaling > 3)
					GCSettings.scaling = 0;
				break;

			case 10:
				DefaultSettings ();
				WaitPrompt("Preferences Reset");
				break;

			case 11:
			case -1: /*** Button B ***/
				SavePrefs(SILENT);
				quit = 1;
				break;

		}
	}
	menu = oldmenu;
}

/****************************************************************************
 * Game Options Menu
 ***************************************************************************/

static int
GameMenu ()
{
	int gamemenuCount = 9;
	char gamemenu[][50] = {
	  "Return to Game",
	  "Reset Game",
	  "Load SRAM", "Save SRAM",
	  "Load Game Snapshot", "Save Game Snapshot",
	  "Reset Zoom",
	  "Weather: 100% sun",
	  "Back to Main Menu"
	};

	int ret, retval = 0;
	int quit = 0;
	int oldmenu = menu;
	menu = 0;

	while (quit == 0)
	{
		// disable SRAM/SNAPSHOT saving/loading if AUTO is on

		if (GCSettings.AutoLoad == 1) // Auto Load SRAM
			gamemenu[2][0] = '\0';
		else if (GCSettings.AutoLoad == 2) // Auto Load SNAPSHOT
			gamemenu[4][0] = '\0';

		if (GCSettings.AutoSave == 1) // Auto Save SRAM
			gamemenu[3][0] = '\0';
		else if (GCSettings.AutoSave == 2) // Auto Save SNAPSHOT
			gamemenu[5][0] = '\0';
		else if (GCSettings.AutoSave == 3) // Auto Save BOTH
		{
			gamemenu[3][0] = '\0';
			gamemenu[5][0] = '\0';
		}
		// disable Reset Zoom if Zooming is off
		if(!GCSettings.Zoom)
			gamemenu[6][0] = '\0';

		// Weather menu if a game with Boktai solar sensor
		if ((RomIdCode & 0xFF)=='U')
			sprintf(gamemenu[7], "Weather: %d%% sun", SunBars*10);
		else
			gamemenu[7][0] = '\0';

		ret = RunMenu (gamemenu, gamemenuCount, "Game Menu");

		switch (ret)
		{
			case 0: // Return to Game
				quit = retval = 1;
				break;

			case 1: // Reset Game
				emulator.emuReset();
				quit = retval = 1;
				break;

			case 2: // Load Battery
				quit = retval = LoadBatteryOrState(GCSettings.SaveMethod, FILE_SRAM, NOTSILENT);
				emulator.emuReset();
				break;

			case 3: // Save Battery
				SaveBatteryOrState(GCSettings.SaveMethod, FILE_SRAM, NOTSILENT);
				break;

			case 4: // Load State
				quit = retval = LoadBatteryOrState(GCSettings.SaveMethod, FILE_SNAPSHOT, NOTSILENT);
				break;

			case 5: // Save State
				SaveBatteryOrState(GCSettings.SaveMethod, FILE_SNAPSHOT, NOTSILENT);
				break;

			case 6:	// Reset Zoom
				zoom_reset ();
				quit = retval = 1;
				break;

			case 7: // Weather
				SunBars++;
				if (SunBars>10) SunBars=0;
				break;

			case -1: // Button B
			case 8: // Return to previous menu
				retval = 0;
				quit = 1;
				break;
		}
	}

	menu = oldmenu;

	return retval;
}

/****************************************************************************
 * Controller Configuration
 ***************************************************************************/
static u32
GetInput (u16 ctrlr_type)
{
	//u32 exp_type;
	u32 pressed;
	pressed=0;
	s8 gc_px = 0;

	while( PAD_ButtonsHeld(0)
#ifdef HW_RVL
	| WPAD_ButtonsHeld(0)
	| AnyKeyDown()
#endif
	) VIDEO_WaitVSync();	// button 'debounce'

	while (pressed == 0 && !AnyKeyDown())
	{
		VIDEO_WaitVSync();
		// get input based on controller type
		if (ctrlr_type == CTRLR_GCPAD)
		{
			pressed = PAD_ButtonsHeld (0);
			gc_px = PAD_SubStickX (0);
		}
#ifdef HW_RVL
		else
		{
			// also needed for keyboard read, to prevent infinite wait when no keyboard
			pressed = WPAD_ButtonsHeld (0);
		}
#endif
		/*** check for exit sequence (c-stick left OR home button) ***/
		if ( (gc_px < -70) || (pressed & WPAD_BUTTON_HOME) || (pressed & WPAD_CLASSIC_BUTTON_HOME))
			return 0;
		if (DownUsbKeys[KB_ESC]) return 0;
		if (ctrlr_type == CTRLR_KEYBOARD)
			pressed = 0;
	}	// end while
	if (DownUsbKeys[KB_ESC]) return 0;
	if (!pressed) {
		for (int i=4; i<=231; i++) {
			if (DownUsbKeys[i]) {
				while (DownUsbKeys[i]) {
					VIDEO_WaitVSync();
				}
				return i;
			}
		}
	}
	while( pressed == (PAD_ButtonsHeld(0)
#ifdef HW_RVL
						| WPAD_ButtonsHeld(0)
#endif
						) ) VIDEO_WaitVSync();

	return pressed;
}	// end GetInput()

static u32
GetButtonMap(u16 ctrlr_type, char* btn_name)
{
	int cfg_text_count = 7;
	char cfg_text[][50] = {
	"Remapping          ",
	"Press Any Button",
	"on the",
	"        ",	// identify controller (must be at least 8 spaces for "keyboard")
	"                   ",
	"Press C-Left or",
	"Home to exit"
	};

	u32 pressed, previous;
	char temp[50] = "";
	uint k;
	pressed = 0; previous = 1;

	switch (ctrlr_type) {
		case CTRLR_NUNCHUK:
			strncpy (cfg_text[3], "NUNCHUK", 8); // 8 spaces in "identify controller" above
			break;
		case CTRLR_CLASSIC:
			strncpy (cfg_text[3], "CLASSIC", 8);
			break;
		case CTRLR_GCPAD:
			strncpy (cfg_text[3], "GC PAD", 8);
			break;
		case CTRLR_WIIMOTE:
			strncpy (cfg_text[3], "WIIMOTE", 8);
			break;
		case CTRLR_KEYBOARD:
			strncpy (cfg_text[3], "KEYBOARD", 8);
	};

	/*** note which button we are remapping ***/
	sprintf (temp, "Remapping ");
	for (k=0; k<9-strlen(btn_name); k++) strcat(temp, " "); // add whitespace padding to align text
	strncat (temp, btn_name, 9);		// snes button we are remapping
	strncpy (cfg_text[0], temp, 19);	// copy this all back to the text we wish to display

	DrawMenu(&cfg_text[0], NULL, cfg_text_count, 1);	// display text

	pressed = GetInput(ctrlr_type);
	return pressed;
}	// end getButtonMap()

static void
ConfigureButtons (u16 ctrlr_type)
{
	int cfg_btns_count = 11;
	char cfg_btns_menu[][50] = {
		"B        -         ",
		"A        -         ",
		"SELECT   -         ",
		"START    -         ",
		"UP       -         ",
		"DOWN     -         ",
		"LEFT     -         ",
		"RIGHT    -         ",
		"L TRIG   -         ",
		"R TRIG   -         ",
		"Return to previous"
	};

	int quit = 0;
	int ret = 0;
	int oldmenu = menu;
	menu = 0;
	char menu_title[50];
	u32 pressed;

	unsigned int* currentpadmap = 0;
	char temp[50] = "";
	int i, j;
	uint k;

	/*** Update Menu Title (based on controller we're configuring) ***/
	switch (ctrlr_type) {
		case CTRLR_NUNCHUK:
			sprintf(menu_title, "VBA     -  NUNCHUK");
			currentpadmap = ncpadmap;
			break;
		case CTRLR_CLASSIC:
			sprintf(menu_title, "VBA     -  CLASSIC");
			currentpadmap = ccpadmap;
			break;
		case CTRLR_GCPAD:
			sprintf(menu_title, "VBA     -   GC PAD");
			currentpadmap = gcpadmap;
			break;
		case CTRLR_WIIMOTE:
			sprintf(menu_title, "VBA     -  WIIMOTE");
			currentpadmap = wmpadmap;
			break;
		case CTRLR_KEYBOARD:
			sprintf(menu_title, "VBA     -  KEYBOARD");
			currentpadmap = kbpadmap;
			break;
	};

	while (quit == 0)
	{
		/*** Update Menu with Current ButtonMap ***/
		for (i=0; i<10; i++) // vba pad has 10 buttons to config (go thru them)
		{
			// get current padmap button name to display
			for ( j=0;
					j < ctrlr_def[ctrlr_type].num_btns &&
					currentpadmap[i] != ctrlr_def[ctrlr_type].map[j].btn	// match padmap button press with button names
				; j++ );

			memset (temp, 0, sizeof(temp));
			strncpy (temp, cfg_btns_menu[i], 12);	// copy vba button information
			if (currentpadmap[i] == ctrlr_def[ctrlr_type].map[j].btn)		// check if a match was made
			{
				for (k=0; k<7-strlen(ctrlr_def[ctrlr_type].map[j].name) ;k++) strcat(temp, " "); // add whitespace padding to align text
				strncat (temp, ctrlr_def[ctrlr_type].map[j].name, 6);		// update button map display
			}
			else
				strcat (temp, "---"); // otherwise, button is 'unmapped'
			strncpy (cfg_btns_menu[i], temp, 19);	// move back updated information

		}

		ret = RunMenu (cfg_btns_menu, cfg_btns_count, menu_title, 16);

		switch (ret)
		{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				/*** Change button map ***/
				// wait for input
				memset (temp, 0, sizeof(temp));
				strncpy(temp, cfg_btns_menu[ret], 6);			// get the name of the vba button we're changing
				pressed = GetButtonMap(ctrlr_type, temp);	// get a button selection from user
				// FIX: check if input is valid for this controller
				if (pressed != 0)	// check if a the button was configured, or if the user exited.
					currentpadmap[ret] = pressed;	// update mapping
				break;

			case -1: /*** Button B ***/
			case 10:
				/*** Return ***/
				quit = 1;
				break;
		}
	}
	menu = oldmenu;
}	// end configurebuttons()

void
ConfigureControllers ()
{
	int ctlrmenucount = 8;
	char ctlrmenu[][50] = {
		"Match Wii Game",
		"Nunchuk",
		"Classic Controller",
		"Wiimote",
		"Gamecube Pad",
		"Keyboard",
		"Save Preferences",
		"Go Back"
	};

	int quit = 0;
	int ret = 0;
	int oldmenu = menu;
	menu = 0;


	// disable unavailable controller options if in GC mode
	#ifndef HW_RVL
		ctlrmenu[1][0] = 0;
		ctlrmenu[2][0] = 0;
		ctlrmenu[3][0] = 0;
		ctlrmenu[5][0] = 0;
	#endif

	while (quit == 0)
	{

		sprintf (ctlrmenu[0], "Match Wii Game: %s",
			GCSettings.WiiControls == true ? " ON" : "OFF");

		/*** Controller Config Menu ***/
        ret = RunMenu (ctlrmenu, ctlrmenucount, "Configure Controllers");

		switch (ret)
		{
			case 0:
				GCSettings.WiiControls ^= 1;
				break;

			case 1:
				/*** Configure Nunchuk ***/
				ConfigureButtons (CTRLR_NUNCHUK);
				break;

			case 2:
				/*** Configure Classic ***/
				ConfigureButtons (CTRLR_CLASSIC);
				break;

			case 3:
				/*** Configure Wiimote ***/
				ConfigureButtons (CTRLR_WIIMOTE);
				break;

			case 4:
				/*** Configure GC Pad ***/
				ConfigureButtons (CTRLR_GCPAD);
				break;

			case 5:
				/*** Configure Keyboard ***/
				ConfigureButtons (CTRLR_KEYBOARD);
				break;

			case 6:
				/*** Save Preferences Now ***/
				SavePrefs(NOTSILENT);
				break;

			case -1: /*** Button B ***/
			case 7:
				/*** Return ***/
				quit = 1;
				break;
		}
	}

	menu = oldmenu;
}

/****************************************************************************
 * Main Menu
 ***************************************************************************/

void
MainMenu (int selectedMenu)
{
	int menucount = 8;
	char menuitems[][50] = {
	  "Choose Game",
	  "Controller Configuration",
	  "Preferences",
	  "Game Menu",
	  "Credits",
	  "DVD Motor Off",
	  "Reset System",
	  "Return to Loader"
	};

	tb_t start,end;
	mftb(&start);

	int quit = 0;
	int ret;

	#ifdef HW_RVL
	// don't show dvd motor off on the wii
	menuitems[5][0] = 0;
	// rename reset/exit items
	sprintf (menuitems[6], "Return to Wii Menu");
	sprintf (menuitems[7], "Return to Homebrew Channel");
	#endif

	// disable game-specific menu items if a ROM isn't loaded
	if (!ROMLoaded)
    	menuitems[3][0] = '\0';
	else
		sprintf (menuitems[3], "Game Menu");

	VIDEO_WaitVSync ();

	while (quit == 0)
	{
		if(selectedMenu >= 0)
		{
			ret = selectedMenu;
			selectedMenu = -1; // default back to main menu
		}
		else
		{
			ret = RunMenu (menuitems, menucount, "Main Menu");
		}

		switch (ret)
		{
			case 0:
				// Load ROM Menu
				quit = LoadManager ();
				break;

			case 1:
				// Configure Controllers
				ConfigureControllers ();
				break;

			case 2:
				// Preferences
				PreferencesMenu ();
				break;

			case 3:
				// Game Options
				quit = GameMenu ();
				break;

			case 4:
				// Credits
				Credits ();
				WaitButtonA ();
                break;

			case 5:
				// turn the dvd motor off (GC only)
				#ifdef HW_DOL
				dvd_motor_off ();
				#endif
				break;

			case 6:
				// Reset the Gamecube/Wii
			    Reboot();
                break;

			case 7:
				ExitToLoader();
				break;

			case -1: // Button B
				// Return to Game
				if(ROMLoaded)
					quit = 1;
				break;
		}
	}

	// Wait for buttons to be released
	int count = 0; // how long we've been waiting for the user to release the button
	while(count < 50 && (
		PAD_ButtonsHeld(0)
		#ifdef HW_RVL
		|| WPAD_ButtonsHeld(0)
		#endif
	))
	{
		VIDEO_WaitVSync();
		count++;
	}

	mftb(&end);
	loadtimeradjust += tb_diff_msec(&end, &start);
}

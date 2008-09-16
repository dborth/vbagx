/****************************************************************************
 * Snes9x 1.51 Nintendo Wii/Gamecube Port
 *
 * softdev July 2006
 * crunchy2 May 2007
 * Tantric September 2008
 *
 * s9xconfig.cpp
 *
 * Configuration parameters are here for easy maintenance.
 * Refer to Snes9x.h for all combinations.
 * The defaults used here are taken directly from porting.html
 ***************************************************************************/

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include "vba.h"
#include "smbop.h"

struct SGCSettings GCSettings;

void
DefaultSettings ()
{
	/************** GameCube/Wii Settings *********************/
	GCSettings.LoadMethod = METHOD_AUTO; // Auto, SD, DVD, USB, Network (SMB)
	GCSettings.SaveMethod = METHOD_AUTO; // Auto, SD, Memory Card Slot A, Memory Card Slot B, USB, Network (SMB)
	sprintf (GCSettings.LoadFolder,"vba/roms"); // Path to game files
	sprintf (GCSettings.SaveFolder,"vba/saves"); // Path to save files
	sprintf (GCSettings.CheatFolder,"vba/cheats"); // Path to cheat files
	GCSettings.AutoLoad = 1;
	GCSettings.AutoSave = 1;

	// custom SMB settings
	strncpy (GCSettings.smbip, "", 15); // IP Address of share server
	strncpy (GCSettings.smbuser, "", 19); // Your share user
	strncpy (GCSettings.smbpwd, "", 19); // Your share user password
	strncpy (GCSettings.smbshare, "", 19); // Share name on server

	GCSettings.smbip[15] = 0;
	GCSettings.smbuser[19] = 0;
	GCSettings.smbpwd[19] = 0;
	GCSettings.smbshare[19] = 0;

	GCSettings.gcip[0] = 0;
	GCSettings.gwip[0] = 0;
	GCSettings.mask[0] = 0;
	GCSettings.smbsvid[0] = 0;
	GCSettings.smbgcid[0] = 0;

	GCSettings.VerifySaves = 0;
}

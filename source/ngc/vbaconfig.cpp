/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vbaconfig.cpp
 *
 * Configuration parameters are here for easy maintenance
 ***************************************************************************/

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include "vba.h"
#include "input.h"

struct SGCSettings GCSettings;

void
DefaultSettings ()
{
	/************** GameCube/Wii Settings *********************/
	ResetControls(); // controller button mappings

	GCSettings.LoadMethod = METHOD_AUTO; // Auto, SD, DVD, USB, Network (SMB)
	GCSettings.SaveMethod = METHOD_AUTO; // Auto, SD, Memory Card Slot A, Memory Card Slot B, USB, Network (SMB)
	sprintf (GCSettings.LoadFolder,"vbagx/roms"); // Path to game files
	sprintf (GCSettings.SaveFolder,"vbagx/saves"); // Path to save files
	sprintf (GCSettings.CheatFolder,"vbagx/cheats"); // Path to cheat files
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

	GCSettings.VerifySaves = 0;
	GCSettings.Zoom = 0;	// zooming default off
	GCSettings.ZoomLevel = 1.0; // zoom level
	GCSettings.render = 1; // Filtered
	GCSettings.widescreen = 0; // no aspect ratio correction
}

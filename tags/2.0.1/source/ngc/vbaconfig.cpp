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

/****************************************************************************
 * FixInvalidSettings
 *
 * Attempts to correct at least some invalid settings - the ones that
 * might cause crashes
 ***************************************************************************/
void FixInvalidSettings()
{
	if(!(GCSettings.ZoomLevel > 0.5 && GCSettings.ZoomLevel < 1.5))
		GCSettings.ZoomLevel = 1.0;
	if(!(GCSettings.xshift > -50 && GCSettings.xshift < 50))
		GCSettings.xshift = 0;
	if(!(GCSettings.yshift > -50 && GCSettings.yshift < 50))
		GCSettings.yshift = 0;
	if(!(GCSettings.MusicVolume >= 0 && GCSettings.MusicVolume <= 100))
		GCSettings.MusicVolume = 40;
	if(!(GCSettings.SFXVolume >= 0 && GCSettings.SFXVolume <= 100))
		GCSettings.SFXVolume = 40;
	if(!(GCSettings.render >= 0 && GCSettings.render < 2))
		GCSettings.render = 1;
	if(!(GCSettings.videomode >= 0 && GCSettings.videomode < 5))
		GCSettings.videomode = 0;
}

/****************************************************************************
 * DefaultSettings
 *
 * Sets all the defaults!
 ***************************************************************************/
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

	GCSettings.WiimoteOrientation = 0;

	GCSettings.VerifySaves = 0;
	GCSettings.ZoomLevel = 1.0; // zoom level
	GCSettings.videomode = 0; // automatic video mode detection
	GCSettings.render = 1; // Filtered
	GCSettings.scaling = 1; // partial stretch
	GCSettings.WiiControls = false; // Match Wii Game

	GCSettings.xshift = 0; // horizontal video shift
	GCSettings.yshift = 0; // vertical video shift
	GCSettings.colorize = 1; // Colorize mono gameboy games

	GCSettings.WiimoteOrientation = 0;
	GCSettings.ExitAction = 0;
	GCSettings.MusicVolume = 40;
	GCSettings.SFXVolume = 40;
}

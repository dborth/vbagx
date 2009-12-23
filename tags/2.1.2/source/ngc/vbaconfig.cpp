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
	if(GCSettings.LoadMethod > 4)
		GCSettings.LoadMethod = DEVICE_AUTO;
	if(GCSettings.SaveMethod > 4)
		GCSettings.SaveMethod = DEVICE_AUTO;	
	if(!(GCSettings.gbaZoomHor > 0.5 && GCSettings.gbaZoomHor < 1.5))
		GCSettings.gbaZoomHor = 1.0;
	if(!(GCSettings.gbaZoomVert > 0.5 && GCSettings.gbaZoomVert < 1.5))
		GCSettings.gbaZoomVert = 1.0;
	if(!(GCSettings.gbZoomHor > 0.5 && GCSettings.gbZoomHor < 1.5))
		GCSettings.gbZoomHor = 1.0;
	if(!(GCSettings.gbZoomVert > 0.5 && GCSettings.gbZoomVert < 1.5))
		GCSettings.gbZoomVert = 1.0;
	if(!(GCSettings.xshift > -50 && GCSettings.xshift < 50))
		GCSettings.xshift = 0;
	if(!(GCSettings.yshift > -50 && GCSettings.yshift < 50))
		GCSettings.yshift = 0;
	if(!(GCSettings.MusicVolume >= 0 && GCSettings.MusicVolume <= 100))
		GCSettings.MusicVolume = 40;
	if(!(GCSettings.SFXVolume >= 0 && GCSettings.SFXVolume <= 100))
		GCSettings.SFXVolume = 40;
	if(!(GCSettings.render >= 0 && GCSettings.render < 3))
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
	memset (&GCSettings, 0, sizeof (GCSettings));
	ResetControls(); // controller button mappings

	GCSettings.LoadMethod = DEVICE_AUTO; // Auto, SD, DVD, USB, Network (SMB)
	GCSettings.SaveMethod = DEVICE_AUTO; // Auto, SD, USB, Network (SMB)
	sprintf (GCSettings.LoadFolder, "%s/roms", APPFOLDER); // Path to game files
	sprintf (GCSettings.SaveFolder, "%s/saves", APPFOLDER); // Path to save files
	sprintf (GCSettings.CheatFolder, "%s/cheats", APPFOLDER); // Path to cheat files
	GCSettings.AutoLoad = 1;
	GCSettings.AutoSave = 1;

	GCSettings.WiimoteOrientation = 0;

	GCSettings.gbaZoomHor = 1.0; // GBA horizontal zoom level
	GCSettings.gbaZoomVert = 1.0; // GBA vertical zoom level
	GCSettings.gbZoomHor = 1.0; // GBA horizontal zoom level
	GCSettings.gbZoomVert = 1.0; // GBA vertical zoom level
	GCSettings.videomode = 0; // automatic video mode detection
	GCSettings.render = 1; // Filtered
	GCSettings.scaling = 1; // partial stretch
	GCSettings.WiiControls = false; // Match Wii Game

	GCSettings.xshift = 0; // horizontal video shift
	GCSettings.yshift = 0; // vertical video shift
	GCSettings.colorize = 0; // Colorize mono gameboy games

	GCSettings.WiimoteOrientation = 0;
	GCSettings.ExitAction = 0;
	GCSettings.MusicVolume = 40;
	GCSettings.SFXVolume = 40;
	GCSettings.Rumble = 1;
}

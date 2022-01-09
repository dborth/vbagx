/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2022
 *
 * preferences.cpp
 *
 * Preferences save/load to XML file
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ogcsys.h>
#include <mxml.h>

#include "vbagx.h"
#include "menu.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"
#include "button_mapping.h"
#include "gamesettings.h"

struct SGCSettings GCSettings;
static gamePalette *palettes = NULL;
static int loadedPalettes = 0;

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ***************************************************************************/
static mxml_node_t *xml = NULL;
static mxml_node_t *data = NULL;
static mxml_node_t *section = NULL;
static mxml_node_t *item = NULL;
static mxml_node_t *elem = NULL;

static mxml_node_t *mxmlFindNewElement(mxml_node_t *parent, const char *nodename, const char *attr=NULL, const char *value=NULL)
{
	mxml_node_t *node = mxmlFindElement(parent, xml, nodename, attr, value, MXML_DESCEND);
	if (!node)
	{
		node = mxmlNewElement(parent, nodename);
		if (attr && value) mxmlElementSetAttr(node, attr, value);
	}
	return node;
}

static char temp[20];

static const char * toStr(int i)
{
	sprintf(temp, "%d", i);
	return temp;
}
static const char * toHex(u32 i)
{
	sprintf(temp, "0x%06X", i);
	return temp;
}
static const char * FtoStr(float i)
{
	sprintf(temp, "%.2f", i);
	return temp;
}

static void createXMLSection(const char * name, const char * description)
{
	section = mxmlNewElement(data, "section");
	mxmlElementSetAttr(section, "name", name);
	mxmlElementSetAttr(section, "description", description);
}

static void createXMLSetting(const char * name, const char * description, const char * value)
{
	item = mxmlNewElement(section, "setting");
	mxmlElementSetAttr(item, "name", name);
	mxmlElementSetAttr(item, "value", value);
	mxmlElementSetAttr(item, "description", description);
}

static void createXMLController(u32 controller[], const char * name, const char * description)
{
	item = mxmlNewElement(section, "controller");
	mxmlElementSetAttr(item, "name", name);
	mxmlElementSetAttr(item, "description", description);

	// create buttons
	for(int i=0; i < MAXJP; i++)
	{
		elem = mxmlNewElement(item, "button");
		mxmlElementSetAttr(elem, "number", toStr(i));
		mxmlElementSetAttr(elem, "assignment", toStr(controller[i]));
	}
}

static const char * XMLSaveCallback(mxml_node_t *node, int where)
{
	const char *name;

	name = node->value.element.name;

	if(where == MXML_WS_BEFORE_CLOSE)
	{
		if(!strcmp(name, "file") || !strcmp(name, "section"))
			return ("\n");
		else if(!strcmp(name, "controller"))
			return ("\n\t");
	}
	if (where == MXML_WS_BEFORE_OPEN)
	{
		if(!strcmp(name, "file"))
			return ("\n");
		else if(!strcmp(name, "section"))
			return ("\n\n");
		else if(!strcmp(name, "setting") || !strcmp(name, "controller"))
			return ("\n\t");
		else if(!strcmp(name, "button"))
			return ("\n\t\t");
	}
	return (NULL);
}

static const char * XMLSavePalCallback(mxml_node_t *node, int where)
{
	const char *name;

	name = node->value.element.name;

	if(where == MXML_WS_BEFORE_CLOSE)
	{
		if(!strcmp(name, "palette") || !strcmp(name, "game"))
			return ("\n");
		else if(!strcmp(name, "bkgr") || !strcmp(name, "wind") || !strcmp(name, "obj0") || !strcmp(name, "obj1"))
			return ("\n\t");
	}
	if (where == MXML_WS_BEFORE_OPEN)
	{
		if(!strcmp(name, "palette"))
			return ("\n");
		else if(!strcmp(name, "game"))
			return ("\n\n");
		else if(!strcmp(name, "bkgr") || !strcmp(name, "wind") || !strcmp(name, "obj0") || !strcmp(name, "obj1"))
			return ("\n\t");
	}
	return (NULL);
}

static int
preparePrefsData ()
{
	xml = mxmlNewXML("1.0");
	mxmlSetWrapMargin(0); // disable line wrapping

	data = mxmlNewElement(xml, "file");
	mxmlElementSetAttr(data, "app", APPNAME);
	mxmlElementSetAttr(data, "version", APPVERSION);

	createXMLSection("File", "File Settings");

	createXMLSetting("AutoLoad", "Auto Load", toStr(GCSettings.AutoLoad));
	createXMLSetting("AutoSave", "Auto Save", toStr(GCSettings.AutoSave));
	createXMLSetting("LoadMethod", "Load Method", toStr(GCSettings.LoadMethod));
	createXMLSetting("SaveMethod", "Save Method", toStr(GCSettings.SaveMethod));
	createXMLSetting("LoadFolder", "Load Folder", GCSettings.LoadFolder);
	createXMLSetting("LastFileLoaded", "Last File Loaded", GCSettings.LastFileLoaded);
	createXMLSetting("SaveFolder", "Save Folder", GCSettings.SaveFolder);
	createXMLSetting("AppendAuto", "Append Auto to .SAV Files", toStr(GCSettings.AppendAuto));
	createXMLSetting("ScreenshotsFolder", "Screenshots Folder", GCSettings.ScreenshotsFolder);
	createXMLSetting("BorderFolder", "SGB Borders Folder", GCSettings.BorderFolder);
	createXMLSetting("CoverFolder", "Covers Folder", GCSettings.CoverFolder);
	createXMLSetting("ArtworkFolder", "Artwork Folder", GCSettings.ArtworkFolder);

	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbip", "Share Computer IP", GCSettings.smbip);
	createXMLSetting("smbshare", "Share Name", GCSettings.smbshare);
	createXMLSetting("smbuser", "Share Username", GCSettings.smbuser);
	createXMLSetting("smbpwd", "Share Password", GCSettings.smbpwd);

	createXMLSection("Video", "Video Settings");

	createXMLSetting("videomode", "Video Mode", toStr(GCSettings.videomode));
	createXMLSetting("gbaZoomHor", "GBA Horizontal Zoom Level", FtoStr(GCSettings.gbaZoomHor));
	createXMLSetting("gbaZoomVert", "GBA Vertical Zoom Level", FtoStr(GCSettings.gbaZoomVert));
	createXMLSetting("gbZoomHor", "GB Horizontal Zoom Level", FtoStr(GCSettings.gbZoomHor));
	createXMLSetting("gbZoomVert", "GB Vertical Zoom Level", FtoStr(GCSettings.gbZoomVert));
	createXMLSetting("gbFixed", "GB Fixed Pixel Ratio", toStr(GCSettings.gbFixed));
	createXMLSetting("gbaFixed", "GBA Fixed Pixel Ratio", toStr(GCSettings.gbaFixed));
	createXMLSetting("render", "Video Filtering", toStr(GCSettings.render));
	createXMLSetting("scaling", "Aspect Ratio Correction", toStr(GCSettings.scaling));
	createXMLSetting("xshift", "Horizontal Video Shift", toStr(GCSettings.xshift));
	createXMLSetting("yshift", "Vertical Video Shift", toStr(GCSettings.yshift));
	createXMLSetting("colorize", "Colorize Mono Gameboy", toStr(GCSettings.colorize));
	createXMLSetting("gbaFrameskip", "GBA Frameskip", toStr(GCSettings.gbaFrameskip));

	createXMLSection("Menu", "Menu Settings");

#ifdef HW_RVL
	createXMLSetting("WiimoteOrientation", "Wiimote Orientation", toStr(GCSettings.WiimoteOrientation));
#endif
	createXMLSetting("ExitAction", "Exit Action", toStr(GCSettings.ExitAction));
	createXMLSetting("MusicVolume", "Music Volume", toStr(GCSettings.MusicVolume));
	createXMLSetting("SFXVolume", "Sound Effects Volume", toStr(GCSettings.SFXVolume));
	createXMLSetting("Rumble", "Rumble", toStr(GCSettings.Rumble));
	createXMLSetting("language", "Language", toStr(GCSettings.language));
	createXMLSetting("PreviewImage", "Preview Image", toStr(GCSettings.PreviewImage));

	createXMLSection("Emulation", "Emulation Settings");

	createXMLSetting("BasicPalette", "Basic Color Palette for GB", toStr(GCSettings.BasicPalette));
	
	createXMLSection("Controller", "Controller Settings");

	createXMLController(btnmap[CTRLR_GCPAD], "gcpadmap", "GameCube Pad");
	createXMLSetting("WiiControls", "Match Wii Game", toStr(GCSettings.WiiControls));
	createXMLController(btnmap[CTRLR_WIIMOTE], "wmpadmap", "Wiimote");
	createXMLController(btnmap[CTRLR_CLASSIC], "ccpadmap", "Classic Controller");
	createXMLController(btnmap[CTRLR_NUNCHUK], "ncpadmap", "Nunchuk");
	createXMLController(btnmap[CTRLR_WUPC], "wupcpadmap", "Wii U Pro Controller");
	createXMLController(btnmap[CTRLR_WIIDRC], "drcpadmap", "Wii U Gamepad");

	createXMLSection("Emulation", "Emulation Settings");

	createXMLSetting("OffsetMinutesUTC", "Offset from UTC (minutes)", toStr(GCSettings.OffsetMinutesUTC));
	createXMLSetting("GBHardware", "Hardware (GB/GBC)", toStr(GCSettings.GBHardware));
	createXMLSetting("SGBBorder", "Border (GB/GBC)", toStr(GCSettings.SGBBorder));

	int datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, XMLSaveCallback);

	mxmlDelete(xml);

	return datasize;
}

static void createXMLPalette(gamePalette *p, bool overwrite, const char *newname = NULL)
{
	if (!newname)
		newname = p->gameName;
	section = mxmlFindElement(xml, xml, "game", "name", newname, MXML_DESCEND);
	if (section && !overwrite)
	{
		return;
	}
	else if (!section)
	{
		section = mxmlNewElement(data, "game");
	}
	mxmlElementSetAttr(section, "name", newname);
	mxmlElementSetAttr(section, "use", "1");
	item = mxmlFindNewElement(section, "bkgr");
	mxmlElementSetAttr(item, "c0", toHex(p->palette[0]));
	mxmlElementSetAttr(item, "c1", toHex(p->palette[1]));
	mxmlElementSetAttr(item, "c2", toHex(p->palette[2]));
	mxmlElementSetAttr(item, "c3", toHex(p->palette[3]));
	item = mxmlFindNewElement(section, "wind");
	mxmlElementSetAttr(item, "c0", toHex(p->palette[4]));
	mxmlElementSetAttr(item, "c1", toHex(p->palette[5]));
	mxmlElementSetAttr(item, "c2", toHex(p->palette[6]));
	mxmlElementSetAttr(item, "c3", toHex(p->palette[7]));
	item = mxmlFindNewElement(section, "obj0");
	mxmlElementSetAttr(item, "c0", toHex(p->palette[8]));
	mxmlElementSetAttr(item, "c1", toHex(p->palette[9]));
	mxmlElementSetAttr(item, "c2", toHex(p->palette[10]));
	item = mxmlFindNewElement(section, "obj1");
	mxmlElementSetAttr(item, "c0", toHex(p->palette[11]));
	mxmlElementSetAttr(item, "c1", toHex(p->palette[12]));
	mxmlElementSetAttr(item, "c2", toHex(p->palette[13]));
}

static int
preparePalData (gamePalette pals[], int palCount)
{
	xml = mxmlNewXML("1.0");
	mxmlSetWrapMargin(0); // disable line wrapping

	data = mxmlNewElement(xml, "palette");
	mxmlElementSetAttr(data, "app", APPNAME);
	mxmlElementSetAttr(data, "version", APPVERSION);
	for (int i=0; i<palCount; i++)
		createXMLPalette(&pals[i], false);

	int datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, XMLSavePalCallback);

	mxmlDelete(xml);

	return datasize;
}

/****************************************************************************
 * loadXMLSetting
 *
 * Load XML elements into variables for an individual variable
 ***************************************************************************/

static void loadXMLSetting(char * var, const char * name, int maxsize)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp)
			snprintf(var, maxsize, "%s", tmp);
	}
}
static void loadXMLSetting(int * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp)
			*var = atoi(tmp);
	}
}
static void loadXMLSetting(float * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp)
			*var = atof(tmp);
	}
}

/****************************************************************************
 * loadXMLController
 *
 * Load XML elements into variables for a controller mapping
 ***************************************************************************/

static void loadXMLController(u32 controller[], const char * name)
{
	item = mxmlFindElement(xml, xml, "controller", "name", name, MXML_DESCEND);

	if(item)
	{
		// populate buttons
		for(int i=0; i < MAXJP; i++)
		{
			elem = mxmlFindElement(item, xml, "button", "number", toStr(i), MXML_DESCEND);
			if(elem)
			{
				const char * tmp = mxmlElementGetAttr(elem, "assignment");
				if(tmp)
					controller[i] = atoi(tmp);
			}
		}
	}
}

static void loadXMLPaletteFromSection(gamePalette &pal)
{
	if (section)
	{
		strncpy(pal.gameName, mxmlElementGetAttr(section, "name"), 17);
		item = mxmlFindElement(section, xml, "bkgr", NULL, NULL, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[0] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[1] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[2] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c3");
			if (tmp)
				pal.palette[3] = strtoul(tmp, NULL, 16);
		}
		item = mxmlFindElement(section, xml, "wind", NULL, NULL, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[4] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[5] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[6] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c3");
			if (tmp)
				pal.palette[7] = strtoul(tmp, NULL, 16);
		}
		item = mxmlFindElement(section, xml, "obj0", NULL, NULL, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[8] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[9] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[10] = strtoul(tmp, NULL, 16);
		}
		item = mxmlFindElement(section, xml, "obj1", NULL, NULL, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[11] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[12] = strtoul(tmp, NULL, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[13] = strtoul(tmp, NULL, 16);
		}
		const char *use = mxmlElementGetAttr(section, "use");
		if (use)
		{
			if (atoi(use) == 0)
				pal.use = 0;
			else
				pal.use = 1;
		}
		else
		{
			pal.use = 1;
		}
	}
}

/****************************************************************************
 * decodePrefsData
 *
 * Decodes preferences - parses XML and loads preferences into the variables
 ***************************************************************************/

static bool
decodePrefsData ()
{
	bool result = false;

	xml = mxmlLoadString(NULL, (char *)savebuffer, MXML_TEXT_CALLBACK);

	if(xml)
	{
		// check settings version
		// we don't do anything with the version #, but we'll store it anyway
		item = mxmlFindElement(xml, xml, "file", "version", NULL, MXML_DESCEND);
		if(item) // a version entry exists
		{
			const char * version = mxmlElementGetAttr(item, "version");

			if(version && strlen(version) == 5)
			{
				// this code assumes version in format X.X.X
				// XX.X.X, X.XX.X, or X.X.XX will NOT work
				int verMajor = version[0] - '0';
				int verMinor = version[2] - '0';
				int verPoint = version[4] - '0';

				// check that the versioning is valid
				if(!(verMajor >= 2 && verMajor <= 9 &&
					verMinor >= 0 && verMinor <= 9 &&
					verPoint >= 0 && verPoint <= 9))
					result = false;
				else
					result = true;
			}
		}

		if(result)
		{
			// File Settings

			loadXMLSetting(&GCSettings.AutoLoad, "AutoLoad");
			loadXMLSetting(&GCSettings.AutoSave, "AutoSave");
			loadXMLSetting(&GCSettings.LoadMethod, "LoadMethod");
			loadXMLSetting(&GCSettings.SaveMethod, "SaveMethod");
			loadXMLSetting(GCSettings.LoadFolder, "LoadFolder", sizeof(GCSettings.LoadFolder));
			loadXMLSetting(GCSettings.LastFileLoaded, "LastFileLoaded", sizeof(GCSettings.LastFileLoaded));
			loadXMLSetting(GCSettings.SaveFolder, "SaveFolder", sizeof(GCSettings.SaveFolder));
			loadXMLSetting(&GCSettings.AppendAuto, "AppendAuto");
			//loadXMLSetting(GCSettings.CheatFolder, "CheatFolder", sizeof(GCSettings.CheatFolder));
			loadXMLSetting(GCSettings.ScreenshotsFolder, "ScreenshotsFolder", sizeof(GCSettings.ScreenshotsFolder));
			loadXMLSetting(GCSettings.BorderFolder, "BorderFolder", sizeof(GCSettings.BorderFolder));
			loadXMLSetting(GCSettings.CoverFolder, "CoverFolder", sizeof(GCSettings.CoverFolder));
			loadXMLSetting(GCSettings.ArtworkFolder, "ArtworkFolder", sizeof(GCSettings.ArtworkFolder));

			// Network Settings

			loadXMLSetting(GCSettings.smbip, "smbip", sizeof(GCSettings.smbip));
			loadXMLSetting(GCSettings.smbshare, "smbshare", sizeof(GCSettings.smbshare));
			loadXMLSetting(GCSettings.smbuser, "smbuser", sizeof(GCSettings.smbuser));
			loadXMLSetting(GCSettings.smbpwd, "smbpwd", sizeof(GCSettings.smbpwd));

			// Video Settings

			loadXMLSetting(&GCSettings.videomode, "videomode");
			loadXMLSetting(&GCSettings.gbaZoomHor, "gbaZoomHor");
			loadXMLSetting(&GCSettings.gbaZoomVert, "gbaZoomVert");
			loadXMLSetting(&GCSettings.gbZoomHor, "gbZoomHor");
			loadXMLSetting(&GCSettings.gbZoomVert, "gbZoomVert");
			loadXMLSetting(&GCSettings.gbaFixed, "gbaFixed");
			loadXMLSetting(&GCSettings.gbFixed, "gbFixed");
			loadXMLSetting(&GCSettings.render, "render");
			loadXMLSetting(&GCSettings.scaling, "scaling");
			loadXMLSetting(&GCSettings.xshift, "xshift");
			loadXMLSetting(&GCSettings.yshift, "yshift");
			loadXMLSetting(&GCSettings.colorize, "colorize");
			loadXMLSetting(&GCSettings.gbaFrameskip, "gbaFrameskip");

			// Menu Settings

			loadXMLSetting(&GCSettings.WiimoteOrientation, "WiimoteOrientation");
			loadXMLSetting(&GCSettings.ExitAction, "ExitAction");
			loadXMLSetting(&GCSettings.MusicVolume, "MusicVolume");
			loadXMLSetting(&GCSettings.SFXVolume, "SFXVolume");
			loadXMLSetting(&GCSettings.Rumble, "Rumble");
			loadXMLSetting(&GCSettings.language, "language");
			loadXMLSetting(&GCSettings.PreviewImage, "PreviewImage");

			// Controller Settings
			loadXMLController(btnmap[CTRLR_GCPAD], "gcpadmap");
			loadXMLSetting(&GCSettings.WiiControls, "WiiControls");
			loadXMLController(btnmap[CTRLR_WIIMOTE], "wmpadmap");
			loadXMLController(btnmap[CTRLR_CLASSIC], "ccpadmap");
			loadXMLController(btnmap[CTRLR_NUNCHUK], "ncpadmap");
			loadXMLController(btnmap[CTRLR_WUPC], "wupcpadmap");
			loadXMLController(btnmap[CTRLR_WIIDRC], "drcpadmap");
			// Emulation Settings
			
			loadXMLSetting(&GCSettings.OffsetMinutesUTC, "OffsetMinutesUTC");
			loadXMLSetting(&GCSettings.GBHardware, "GBHardware");
			loadXMLSetting(&GCSettings.SGBBorder, "SGBBorder");
			loadXMLSetting(&GCSettings.BasicPalette, "BasicPalette");
		}
		mxmlDelete(xml);
	}
	return result;
}

static bool
decodePalsData ()
{
	bool result = false;

	xml = mxmlLoadString(NULL, (char *) savebuffer, MXML_TEXT_CALLBACK);

	if (xml)
	{
		// count number of palettes in file
		loadedPalettes = 0;
		item = mxmlFindElement(xml, xml, "palette", NULL, NULL, MXML_DESCEND);
		for (section = mxmlFindElement(item, xml, "game", NULL, NULL,
				MXML_DESCEND); section; section = mxmlFindElement(section, xml,
				"game", NULL, NULL, MXML_NO_DESCEND))
		{
			loadedPalettes++;
		}
		// Allocate enough memory for all palettes in file, plus all hardcoded palettes,
		// plus one new palette
		if (palettes)
			free(palettes);

		palettes = (gamePalette *)malloc(sizeof(gamePalette)*loadedPalettes);
		// Load all palettes in file, hardcoded palettes are added later
		int i = 0;
		for (section = mxmlFindElement(item, xml, "game", NULL, NULL,
				MXML_DESCEND); section; section = mxmlFindElement(section, xml,
				"game", NULL, NULL, MXML_NO_DESCEND))
		{
			loadXMLPaletteFromSection(palettes[i]);
			i++;
		}
		mxmlDelete(xml);
	}
	return result;
}

/****************************************************************************
 * FixInvalidSettings
 *
 * Attempts to correct at least some invalid settings - the ones that
 * might cause crashes
 ***************************************************************************/
void FixInvalidSettings()
{
	if(GCSettings.LoadMethod > 7)
		GCSettings.LoadMethod = DEVICE_AUTO;
	if(GCSettings.SaveMethod > 7)
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
		GCSettings.MusicVolume = 20;
	if(!(GCSettings.SFXVolume >= 0 && GCSettings.SFXVolume <= 100))
		GCSettings.SFXVolume = 40;
	if(GCSettings.language < 0 || GCSettings.language >= LANG_LENGTH)
		GCSettings.language = LANG_ENGLISH;
	if(!(GCSettings.render >= 0 && GCSettings.render < 5))
		GCSettings.render = 1;
	if(!(GCSettings.videomode >= 0 && GCSettings.videomode < 7))
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
	sprintf (GCSettings.ScreenshotsFolder, "%s/screenshots", APPFOLDER);
	sprintf (GCSettings.BorderFolder, "%s/borders", APPFOLDER);
	sprintf (GCSettings.CoverFolder, "%s/covers", APPFOLDER); // Path to cover files
	sprintf (GCSettings.ArtworkFolder, "%s/artwork", APPFOLDER); // Path to artwork files

	GCSettings.AutoLoad = 1;
	GCSettings.AutoSave = 1;
	GCSettings.AppendAuto = 1;

	GCSettings.WiimoteOrientation = 0;

	GCSettings.gbaZoomHor = 1.0; // GBA horizontal zoom level
	GCSettings.gbaZoomVert = 1.0; // GBA vertical zoom level
	GCSettings.gbZoomHor = 1.0; // GBA horizontal zoom level
	GCSettings.gbZoomVert = 1.0; // GBA vertical zoom level
	GCSettings.gbFixed = 0; // not fixed - use zoom level
	GCSettings.gbaFixed = 0; // not fixed - use zoom level
	GCSettings.videomode = 0; // automatic video mode detection
	GCSettings.render = 1; // Filtered
	GCSettings.scaling = 1; // partial stretch
	GCSettings.WiiControls = false; // Match Wii Game

	GCSettings.xshift = 0; // horizontal video shift
	GCSettings.yshift = 0; // vertical video shift
	GCSettings.colorize = 0; // Colorize mono gameboy games
	GCSettings.gbaFrameskip = 1; // Turn auto-frameskip on for GBA games

	GCSettings.WiimoteOrientation = 0;
	GCSettings.ExitAction = 0;
	GCSettings.AutoloadGame = 0;
	GCSettings.MusicVolume = 20;
	GCSettings.SFXVolume = 40;
	GCSettings.Rumble = 1;
	GCSettings.PreviewImage = 0;
	
	GCSettings.BasicPalette = 0;
	
#ifdef HW_RVL
	GCSettings.language = CONF_GetLanguage();

	if(GCSettings.language == LANG_TRAD_CHINESE)
		GCSettings.language = LANG_SIMP_CHINESE;
#else
	GCSettings.language = LANG_ENGLISH;
#endif
	GCSettings.OffsetMinutesUTC = 0;
	GCSettings.GBHardware = 0;
	GCSettings.SGBBorder = 0;
}


/****************************************************************************
 * Save Preferences
 ***************************************************************************/
static char prefpath[MAXPATHLEN] = { 0 };

bool
SavePrefs (bool silent)
{
	char filepath[MAXPATHLEN];
	int datasize;
	int offset = 0;
	int device = 0;
	
	if(prefpath[0] != 0)
	{
		sprintf(filepath, "%s/%s", prefpath, PREF_FILE_NAME);
		FindDevice(filepath, &device);
	}
	else if(appPath[0] != 0)
	{
		sprintf(filepath, "%s/%s", appPath, PREF_FILE_NAME);
		strcpy(prefpath, appPath);
		FindDevice(filepath, &device);
	}
	else
	{
		device = autoSaveMethod(silent);
		
		if(device == 0)
			return false;
		
		sprintf(filepath, "%s%s", pathPrefix[device], APPFOLDER);
						
		DIR *dir = opendir(filepath);
		if (!dir)
		{
			if(mkdir(filepath, 0777) != 0)
				return false;
			sprintf(filepath, "%s%s/roms", pathPrefix[device], APPFOLDER);
			if(mkdir(filepath, 0777) != 0)
				return false;
			sprintf(filepath, "%s%s/saves", pathPrefix[device], APPFOLDER);
			if(mkdir(filepath, 0777) != 0)
				return false;
		}
		else
		{
			closedir(dir);
		}
		sprintf(filepath, "%s%s/%s", pathPrefix[device], APPFOLDER, PREF_FILE_NAME);
		sprintf(prefpath, "%s%s", pathPrefix[device], APPFOLDER);
	}
	
	if(device == 0)
		return false;

	if (!silent)
		ShowAction ("Saving preferences...");

	FixInvalidSettings();

	AllocSaveBuffer ();
	datasize = preparePrefsData ();

	offset = SaveFile(filepath, datasize, silent);

	FreeSaveBuffer ();

	CancelAction();

	if (offset > 0)
	{
		if (!silent)
			InfoPrompt("Preferences saved");
		return true;
	}
	return false;
}

/****************************************************************************
 * Load Preferences from specified filepath
 ***************************************************************************/
bool
LoadPrefsFromMethod (char * path)
{
	bool retval = false;
	int offset = 0;
	char filepath[MAXPATHLEN];
	sprintf(filepath, "%s/%s", path, PREF_FILE_NAME);

	AllocSaveBuffer ();

	offset = LoadFile(filepath, SILENT);

	if (offset > 0)
		retval = decodePrefsData ();

	FreeSaveBuffer ();

	if(retval)
	{
		strcpy(prefpath, path);

		if(appPath[0] == 0)
			strcpy(appPath, prefpath);
	}

	return retval;
}

/****************************************************************************
 * Load Preferences
 * Checks sources consecutively until we find a preference file
 ***************************************************************************/
static bool prefLoaded = false;

bool LoadPrefs()
{
	if(prefLoaded) // already attempted loading
		return true;

	bool prefFound = false;
	char filepath[5][MAXPATHLEN];
	int numDevices;
	
#ifdef HW_RVL
	numDevices = 5;
	sprintf(filepath[0], "%s", appPath);
	sprintf(filepath[1], "sd:/apps/%s", APPFOLDER);
	sprintf(filepath[2], "usb:/apps/%s", APPFOLDER);
	sprintf(filepath[3], "sd:/%s", APPFOLDER);
	sprintf(filepath[4], "usb:/%s", APPFOLDER);

	for(int i=0; i<numDevices; i++)
	{
		prefFound = LoadPrefsFromMethod(filepath[i]);
		
		if(prefFound)
			break;
	}
#else
	if(ChangeInterface(DEVICE_SD_SLOTA, SILENT)) {
		sprintf(filepath[0], "carda:/%s", APPFOLDER);
		prefFound = LoadPrefsFromMethod(filepath[0]);
	}
	else if(ChangeInterface(DEVICE_SD_SLOTB, SILENT)) {
		sprintf(filepath[0], "cardb:/%s", APPFOLDER);
		prefFound = LoadPrefsFromMethod(filepath[0]);
	}
	else if(ChangeInterface(DEVICE_SD_PORT2, SILENT)) {
		sprintf(filepath[0], "port2:/%s", APPFOLDER);
		prefFound = LoadPrefsFromMethod(filepath[0]);
	}
#endif

	prefLoaded = true; // attempted to load preferences

	if(prefFound)
		FixInvalidSettings();

	// attempt to create directories if they don't exist
	if(GCSettings.LoadMethod == DEVICE_SD || GCSettings.LoadMethod == DEVICE_USB) {
		char dirPath[MAXPATHLEN];
		sprintf(dirPath, "%s%s", pathPrefix[GCSettings.LoadMethod], GCSettings.ScreenshotsFolder);
		CreateDirectory(dirPath);
		sprintf(dirPath, "%s%s", pathPrefix[GCSettings.LoadMethod], GCSettings.CoverFolder);
		CreateDirectory(dirPath);
		sprintf(dirPath, "%s%s", pathPrefix[GCSettings.LoadMethod], GCSettings.ArtworkFolder);
		CreateDirectory(dirPath);
	}

#ifdef HW_RVL
	bg_music = (u8 * )bg_music_ogg;
	bg_music_size = bg_music_ogg_size;
	LoadBgMusic();
#endif

	ChangeLanguage();
	return prefFound;
}

bool SavePalettes(bool silent)
{
	char filepath[1024];
	int datasize;
	int offset = 0;

	if(prefpath[0] == 0)
		return false;

	sprintf(filepath, "%s/%s", prefpath, PAL_FILE_NAME);

	// Now create the XML palette file

	if (!silent)
		ShowAction("Saving palette...");

	AllocSaveBuffer();
	datasize = preparePalData(palettes, loadedPalettes);

	offset = SaveFile(filepath, datasize, silent);

	FreeSaveBuffer();

	CancelAction();

	if (offset > 0)
	{
		if (!silent)
			InfoPrompt("Palette saved");
		return true;
	}
	return false;
}

static void AddPalette(gamePalette pal, const char *gameName, bool overwrite)
{
	for (int i=0; i < loadedPalettes; i++)
		if (strcmp(palettes[i].gameName, gameName)==0)
		{
			if (overwrite)
			{
				palettes[i] = pal;
				strncpy(palettes[i].gameName, gameName, 17);
				return;
			}
			else
			{
				return;
			}
		}

	palettes = (gamePalette *)realloc(palettes, sizeof(gamePalette)*(loadedPalettes+1));
	palettes[loadedPalettes] = pal;
	strncpy(palettes[loadedPalettes].gameName, gameName, 17);
	loadedPalettes++;
}

bool SavePaletteAs(bool silent, const char *name)
{
	AddPalette(CurrentPalette, name, true);
	return SavePalettes(silent);
}

/****************************************************************************
 * Load Palettes
 ***************************************************************************/
bool LoadPalettes()
{
	bool retval = false;
	int offset = 0;
	char filepath[MAXPATHLEN];

	AllocSaveBuffer ();

	sprintf(filepath, "%s/%s", prefpath, PAL_FILE_NAME);
	offset = LoadFile(filepath, SILENT);

	if (offset > 0)
		retval = decodePalsData ();

	FreeSaveBuffer ();

	// add hard-coded palettes
	for (int i=0; i<gamePalettesCount; i++)
		AddPalette(gamePalettes[i], gamePalettes[i].gameName, false);

	if (!retval)
		retval = SavePalettes(SILENT);

	return retval;
}

void SetPalette(const char *gameName)
{
	// Load existing palette
	int snum = -1;
	for (int i = 0; i < loadedPalettes; i++)
	{
		if(strcmp(gameName, palettes[i].gameName)==0)
		{
			snum = i;
			break;
		}
	}
	// match found!
	if(snum >= 0)
	{
		CurrentPalette = palettes[snum];
	}
	else
	// no match, use the default palette
	{
		for (int i = 0; i < loadedPalettes; i++)
		{
			if(strcmp(gameName, "default")==0)
			{
				snum = i;
				break;
			}
		}
		if(snum >= 0)
		{
			CurrentPalette = palettes[snum];
		}
		else
		{
			CurrentPalette = palettes[0];
		}
		// DON'T add this game to the palette list
	}
}

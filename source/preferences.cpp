/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * preferences.cpp
 *
 * Preferences save/load to XML file
 ***************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <mxml.h>
#if defined(HW_RVL) || defined(HW_DOL)
#include <ogc/conf.h>
#include <ogc/system.h>
#endif

#include "vbagx.h"
#include "menu.h"
#include "fileop.h"
#include "video.h"
#include "filebrowser.h"
#include "vbasupport.h"
#include "input.h"
#include "button_mapping.h"
#include "gamesettings.h"

#if defined(HW_RVL) || defined(HW_DOL)
#include "drivers/ogc/WiiPlatform.h"
#include "drivers/ogc/GameCubePlatform.h"
#include "drivers/ogc/videofilters.h"
#endif

struct SSettings Settings;
static gamePalette *palettes = nullptr;
static int loadedPalettes = 0;

/****************************************************************************
 * Prepare Preferences Data
 *
 * This sets up the save buffer for saving.
 ***************************************************************************/
static mxml_node_t *xml = nullptr;
static mxml_node_t *data = nullptr;
static mxml_node_t *section = nullptr;
static mxml_node_t *item = nullptr;
static mxml_node_t *elem = nullptr;

static mxml_node_t *mxmlFindNewElement(mxml_node_t *parent, const char *nodename, const char *attr=nullptr, const char *value=nullptr)
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

static const char* BtoStr(bool b)
{
    return b ? "1" : "0";
}
static const char * toStr(int i)
{
	sprintf(temp, "%d", i);
	return temp;
}
static const char * toHex(uint32_t i)
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

static void createXMLController(uint32_t controller[], const char * name, const char * description)
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

	name = mxmlGetElement(node);

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
	return (nullptr);
}

static const char * XMLSavePalCallback(mxml_node_t *node, int where)
{
	const char *name;

	name = mxmlGetElement(node);

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
	return (nullptr);
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

	createXMLSetting("AutoLoad", "Auto Load", toStr(Settings.AutoLoad));
	createXMLSetting("AutoSave", "Auto Save", toStr(Settings.AutoSave));
	createXMLSetting("LoadMethod", "Load Method", toStr(Settings.LoadMethod));
	createXMLSetting("SaveMethod", "Save Method", toStr(Settings.SaveMethod));
	createXMLSetting("LoadFolder", "Load Folder", Settings.LoadFolder);
	createXMLSetting("LastFileLoaded", "Last File Loaded", Settings.LastFileLoaded);
	createXMLSetting("SaveFolder", "Save Folder", Settings.SaveFolder);
	createXMLSetting("AppendAuto", "Append Auto to .SAV Files", BtoStr(Settings.AppendAuto));
	createXMLSetting("CheatFolder", "Cheats Folder", Settings.CheatFolder);
	createXMLSetting("ScreenshotsFolder", "Screenshots Folder", Settings.ScreenshotsFolder);
	createXMLSetting("BorderFolder", "SGB Borders Folder", Settings.BorderFolder);
	createXMLSetting("CoverFolder", "Covers Folder", Settings.CoverFolder);
	createXMLSetting("ArtworkFolder", "Artwork Folder", Settings.ArtworkFolder);

	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbip", "Share Computer IP", Settings.smbip);
	createXMLSetting("smbshare", "Share Name", Settings.smbshare);
	createXMLSetting("smbuser", "Share Username", Settings.smbuser);
	createXMLSetting("smbpwd", "Share Password", Settings.smbpwd);

	createXMLSection("Video", "Video Settings");

	createXMLSetting("videoMode", "Output Mode", toStr(Settings.videoMode));
	createXMLSetting("videoAspectRatioCorrection", "Aspect Ratio Correction", toStr(Settings.videoAspectRatioCorrection));
	createXMLSetting("videoBilinearFilter", "Bilinear Filtering", BtoStr(Settings.videoBilinearFilter));
	createXMLSetting("videoHardwareSoften", "Hardware Soften", toStr(Settings.videoHardwareSoften));
	createXMLSetting("videoScanlines", "Scanlines", BtoStr(Settings.videoScanlines));
	createXMLSetting("videoUpscalingFilter", "Upscaling Filter Method", toStr(Settings.videoUpscalingFilter));
	createXMLSetting("gbaZoomHor", "GBA Horizontal Zoom Level", FtoStr(Settings.gbaZoomHor));
	createXMLSetting("gbaZoomVert", "GBA Vertical Zoom Level", FtoStr(Settings.gbaZoomVert));
	createXMLSetting("gbZoomHor", "GB Horizontal Zoom Level", FtoStr(Settings.gbZoomHor));
	createXMLSetting("gbZoomVert", "GB Vertical Zoom Level", FtoStr(Settings.gbZoomVert));
	createXMLSetting("gbFixed", "GB Fixed Pixel Ratio", toStr(Settings.gbFixed));
	createXMLSetting("gbaFixed", "GBA Fixed Pixel Ratio", toStr(Settings.gbaFixed));
	createXMLSetting("videoXshift", "Horizontal Video Shift", toStr(Settings.videoXshift));
	createXMLSetting("videoYshift", "Vertical Video Shift", toStr(Settings.videoYshift));

	createXMLSection("Menu", "Menu Settings");

#ifdef HW_RVL
	createXMLSetting("wiimoteOrientation", "Wiimote Orientation", toStr(Settings.wiimoteOrientation));
#endif
#if defined(HW_RVL) || defined(HW_DOL)
	createXMLSetting("ExitAction", "Exit Action", toStr(Settings.ExitAction));
#endif
	createXMLSetting("MusicVolume", "Music Volume", toStr(Settings.MusicVolume));
	createXMLSetting("SFXVolume", "Sound Effects Volume", toStr(Settings.SFXVolume));
	createXMLSetting("Rumble", "Rumble", BtoStr(Settings.Rumble));
	createXMLSetting("language", "Language", toStr(Settings.language));
	createXMLSetting("PreviewImage", "Preview Image", toStr(Settings.PreviewImage));

	createXMLSection("Emulation", "Emulation Settings");

	createXMLSetting("DynamicRecompilation", "Dynamic Recompilation (JIT)", BtoStr(Settings.DynamicRecompilation));
	createXMLSetting("gbaFrameskip", "GBA Frameskip", BtoStr(Settings.gbaFrameskip));

	createXMLSetting("GBHardware", "Hardware (GB/GBC)", toStr(Settings.GBHardware));
	createXMLSetting("SGBBorder", "Border (GB/GBC)", toStr(Settings.SGBBorder));
	createXMLSetting("BasicPalette", "Basic Color Palette for GB", toStr(Settings.BasicPalette));
	createXMLSetting("colorize", "Colorize Mono Gameboy", BtoStr(Settings.colorize));

	createXMLSetting("DisplayFrameRate", "Show Framerate", toStr(Settings.DisplayFrameRate));
	createXMLSetting("TurboModeEnabled", "Turbo Mode Enabled", BtoStr(Settings.TurboModeEnabled));
	createXMLSetting("OffsetMinutesUTC", "Offset from UTC (minutes)", toStr(Settings.OffsetMinutesUTC));

	createXMLSection("Controller", "Controller Settings");

	createXMLController(btnmap[INPUT_HW_GAMECUBE], "gcpadmapping", "GameCube Pad");
	createXMLSetting("WiiControls", "Match Wii Game", BtoStr(Settings.WiiControls));
	createXMLController(btnmap[INPUT_HW_WIIMOTE], "wmpadmapping", "Wiimote");
	createXMLController(btnmap[INPUT_HW_CLASSIC], "ccpadmapping", "Classic Controller");
	createXMLController(btnmap[INPUT_HW_NUNCHUK], "ncpadmapping", "Nunchuk");
	createXMLController(btnmap[INPUT_HW_WUPC], "wupcpadmapping", "Wii U Pro Controller");
	createXMLController(btnmap[INPUT_HW_DRC], "drcpadmapping", "Wii U Gamepad");

	int datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, XMLSaveCallback);

	mxmlDelete(xml);

	return datasize;
}

static void createXMLPalette(gamePalette *p, bool overwrite, const char *newname = nullptr)
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
static void loadXMLSetting(bool * var, const char * name)
{
	item = mxmlFindElement(xml, xml, "setting", "name", name, MXML_DESCEND);
	if(item)
	{
		const char * tmp = mxmlElementGetAttr(item, "value");
		if(tmp) {
			if (strcmp(tmp, "1") == 0 || strcasecmp(tmp, "true") == 0)
				*var = true;
			else
				*var = false;
		}
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

static void loadXMLController(uint32_t controller[], const char * name)
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
		strncpy(pal.gameName, mxmlElementGetAttr(section, "name"), sizeof(pal.gameName) - 1);
		pal.gameName[sizeof(pal.gameName) - 1] = 0;
		item = mxmlFindElement(section, xml, "bkgr", nullptr, nullptr, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[0] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[1] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[2] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c3");
			if (tmp)
				pal.palette[3] = strtoul(tmp, nullptr, 16);
		}
		item = mxmlFindElement(section, xml, "wind", nullptr, nullptr, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[4] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[5] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[6] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c3");
			if (tmp)
				pal.palette[7] = strtoul(tmp, nullptr, 16);
		}
		item = mxmlFindElement(section, xml, "obj0", nullptr, nullptr, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[8] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[9] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[10] = strtoul(tmp, nullptr, 16);
		}
		item = mxmlFindElement(section, xml, "obj1", nullptr, nullptr, MXML_DESCEND);
		if (item)
		{
			const char * tmp = mxmlElementGetAttr(item, "c0");
			if (tmp)
				pal.palette[11] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c1");
			if (tmp)
				pal.palette[12] = strtoul(tmp, nullptr, 16);
			tmp = mxmlElementGetAttr(item, "c2");
			if (tmp)
				pal.palette[13] = strtoul(tmp, nullptr, 16);
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

void ApplySettings() {
	platform->getInput()->setWiimoteOrientation(Settings.wiimoteOrientation);
	platform->getInput()->setRumbleEnabled(Settings.Rumble);
	GuiSound::setDefaultVolume(SOUND::OGG, Settings.MusicVolume);
	GuiSound::setDefaultVolume(SOUND::PCM, Settings.SFXVolume);
	platform->getVideo()->startMenuVideo();
	ChangeLanguage();
	InitialisePalette();
}

/****************************************************************************
 * decodePrefsData
 *
 * Decodes preferences - parses XML and loads preferences into the variables
 ***************************************************************************/

static bool
decodePrefsData ()
{
	xml = mxmlLoadString(nullptr, (char *)savebuffer, MXML_TEXT_CALLBACK);

	if(!xml) {
		return false;
	}

	// File Settings

	loadXMLSetting(&Settings.AutoLoad, "AutoLoad");
	loadXMLSetting(&Settings.AutoSave, "AutoSave");
	loadXMLSetting(&Settings.LoadMethod, "LoadMethod");
	loadXMLSetting(&Settings.SaveMethod, "SaveMethod");
	loadXMLSetting(Settings.LoadFolder, "LoadFolder", sizeof(Settings.LoadFolder));
	loadXMLSetting(Settings.LastFileLoaded, "LastFileLoaded", sizeof(Settings.LastFileLoaded));
	loadXMLSetting(Settings.SaveFolder, "SaveFolder", sizeof(Settings.SaveFolder));
	loadXMLSetting(&Settings.AppendAuto, "AppendAuto");
	loadXMLSetting(Settings.CheatFolder, "CheatFolder", sizeof(Settings.CheatFolder));
	loadXMLSetting(Settings.ScreenshotsFolder, "ScreenshotsFolder", sizeof(Settings.ScreenshotsFolder));
	loadXMLSetting(Settings.BorderFolder, "BorderFolder", sizeof(Settings.BorderFolder));
	loadXMLSetting(Settings.CoverFolder, "CoverFolder", sizeof(Settings.CoverFolder));
	loadXMLSetting(Settings.ArtworkFolder, "ArtworkFolder", sizeof(Settings.ArtworkFolder));

	// Network Settings

	loadXMLSetting(Settings.smbip, "smbip", sizeof(Settings.smbip));
	loadXMLSetting(Settings.smbshare, "smbshare", sizeof(Settings.smbshare));
	loadXMLSetting(Settings.smbuser, "smbuser", sizeof(Settings.smbuser));
	loadXMLSetting(Settings.smbpwd, "smbpwd", sizeof(Settings.smbpwd));

	// Video Settings

	loadXMLSetting(&Settings.videoMode, "videoMode");
	loadXMLSetting(&Settings.videoAspectRatioCorrection, "videoAspectRatioCorrection");
	loadXMLSetting(&Settings.videoBilinearFilter, "videoBilinearFilter");
	loadXMLSetting(&Settings.videoHardwareSoften, "videoHardwareSoften");
	loadXMLSetting(&Settings.videoUpscalingFilter, "videoUpscalingFilter");
	loadXMLSetting(&Settings.videoScanlines, "videoScanlines");
	loadXMLSetting(&Settings.gbaZoomHor, "gbaZoomHor");
	loadXMLSetting(&Settings.gbaZoomVert, "gbaZoomVert");
	loadXMLSetting(&Settings.gbZoomHor, "gbZoomHor");
	loadXMLSetting(&Settings.gbZoomVert, "gbZoomVert");
	loadXMLSetting(&Settings.gbaFixed, "gbaFixed");
	loadXMLSetting(&Settings.gbFixed, "gbFixed");
	loadXMLSetting(&Settings.videoXshift, "videoXshift");
	loadXMLSetting(&Settings.videoYshift, "videoYshift");

	// Menu Settings

#ifdef HW_RVL
	loadXMLSetting(&Settings.wiimoteOrientation, "WiimoteOrientation");
#endif
#if defined(HW_RVL) || defined(HW_DOL)
	loadXMLSetting(&Settings.ExitAction, "ExitAction");
#endif
	loadXMLSetting(&Settings.MusicVolume, "MusicVolume");
	loadXMLSetting(&Settings.SFXVolume, "SFXVolume");
	loadXMLSetting(&Settings.Rumble, "Rumble");
	loadXMLSetting(&Settings.language, "language");
	loadXMLSetting(&Settings.PreviewImage, "PreviewImage");

	// Controller Settings

	loadXMLController(btnmap[INPUT_HW_GAMECUBE], "gcpadmapping");
	loadXMLSetting(&Settings.WiiControls, "WiiControls");
	loadXMLController(btnmap[INPUT_HW_WIIMOTE], "wmpadmapping");
	loadXMLController(btnmap[INPUT_HW_CLASSIC], "ccpadmapping");
	loadXMLController(btnmap[INPUT_HW_NUNCHUK], "ncpadmapping");
	loadXMLController(btnmap[INPUT_HW_WUPC], "wupcpadmapping");
	loadXMLController(btnmap[INPUT_HW_DRC], "drcpadmapping");

	// Emulation Settings

	loadXMLSetting(&Settings.DynamicRecompilation, "DynamicRecompilation");
	loadXMLSetting(&Settings.gbaFrameskip, "gbaFrameskip");

	loadXMLSetting(&Settings.GBHardware, "GBHardware");
	loadXMLSetting(&Settings.SGBBorder, "SGBBorder");
	loadXMLSetting(&Settings.colorize, "colorize");
	loadXMLSetting(&Settings.BasicPalette, "BasicPalette");

	loadXMLSetting(&Settings.DisplayFrameRate, "DisplayFrameRate");
	loadXMLSetting(&Settings.TurboModeEnabled, "TurboModeEnabled");
	loadXMLSetting(&Settings.OffsetMinutesUTC, "OffsetMinutesUTC");

	mxmlDelete(xml);
	return true;
}

static bool
decodePalsData ()
{
	xml = mxmlLoadString(nullptr, (char *) savebuffer, MXML_TEXT_CALLBACK);

	if (!xml) {
		return false;
	}

	// count number of palettes in file
	loadedPalettes = 0;
	item = mxmlFindElement(xml, xml, "palette", nullptr, nullptr, MXML_DESCEND);
	for (section = mxmlFindElement(item, xml, "game", nullptr, nullptr,
			MXML_DESCEND); section; section = mxmlFindElement(section, xml,
			"game", nullptr, nullptr, MXML_NO_DESCEND))
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
	for (section = mxmlFindElement(item, xml, "game", nullptr, nullptr,
			MXML_DESCEND); section; section = mxmlFindElement(section, xml,
			"game", nullptr, nullptr, MXML_NO_DESCEND))
	{
		loadXMLPaletteFromSection(palettes[i]);
		i++;
	}
	mxmlDelete(xml);
	return true;
}

/****************************************************************************
 * FixInvalidSettings
 *
 * Attempts to correct at least some invalid settings - the ones that
 * might cause crashes
 ***************************************************************************/
void FixInvalidSettings()
{
	if(!isValidLoadDevice(Settings.LoadMethod))
		Settings.LoadMethod = DEVICE_AUTO;
	if(!isValidSaveDevice(Settings.SaveMethod))
		Settings.SaveMethod = DEVICE_AUTO;

	if(strlen(Settings.smbshare) == 0 || strlen(Settings.smbip) == 0) {
		if(Settings.LoadMethod == DEVICE_SMB) {
			Settings.LoadMethod = DEVICE_AUTO;
		}
		if(Settings.SaveMethod == DEVICE_SMB) {
			Settings.SaveMethod = DEVICE_AUTO;
		}
	}

	if(!(Settings.gbaZoomHor >= 0.5 && Settings.gbaZoomHor <= 1.6))
		Settings.gbaZoomHor = 1.0;
	if(!(Settings.gbaZoomVert >= 0.5 && Settings.gbaZoomVert <= 1.6))
		Settings.gbaZoomVert = 1.0;
	if(!(Settings.gbZoomHor >= 0.5 && Settings.gbZoomHor <= 1.6))
		Settings.gbZoomHor = 1.0;
	if(!(Settings.gbZoomVert >= 0.5 && Settings.gbZoomVert <= 1.6))
		Settings.gbZoomVert = 1.0;
	if(!(Settings.videoXshift > -50 && Settings.videoXshift < 50))
		Settings.videoXshift = 0;
	if(!(Settings.videoYshift > -50 && Settings.videoYshift < 50))
		Settings.videoYshift = 0;
	if(!(Settings.MusicVolume >= 0 && Settings.MusicVolume <= 100))
		Settings.MusicVolume = 20;
	if(!(Settings.SFXVolume >= 0 && Settings.SFXVolume <= 100))
		Settings.SFXVolume = 40;
	if(Settings.language < LANG_JAPANESE || Settings.language >= LANG_LENGTH)
		Settings.language = LANG_ENGLISH;
	if(!(Settings.videoHardwareSoften >= VIDEO_HW_SOFTEN_OFF && Settings.videoHardwareSoften < VIDEO_HW_SOFTEN_LENGTH))
		Settings.videoHardwareSoften = VIDEO_HW_SOFTEN_AUTO;
#if defined(HW_RVL) || defined(HW_DOL)
	if(!(Settings.videoUpscalingFilter >= FILTER_NONE && Settings.videoUpscalingFilter <= NUM_FILTERS))
		Settings.videoUpscalingFilter = FILTER_NONE;
#endif
	if(!(Settings.videoAspectRatioCorrection >= SCALING_MAINTAIN_ASPECT && Settings.videoAspectRatioCorrection < SCALING_LENGTH))
		Settings.videoAspectRatioCorrection = SCALING_MAINTAIN_ASPECT;
	if(!(Settings.videoMode >= VIDEOMODE_AUTO && Settings.videoMode < VIDEOMODE_LENGTH))
		Settings.videoMode = VIDEOMODE_AUTO;
	if(!(Settings.DisplayFrameRate >= FRAMERATE_OFF && Settings.DisplayFrameRate < FRAMERATE_LENGTH))
		Settings.DisplayFrameRate = FRAMERATE_OFF;
	if(!(Settings.wiimoteOrientation >= WIIMOTE_ORIENTATION_AUTO && Settings.wiimoteOrientation < WIIMOTE_ORIENTATION_LENGTH))
		Settings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
}

/****************************************************************************
 * DefaultSettings
 *
 * Sets all the defaults!
 ***************************************************************************/
void DefaultSettings()
{
	memset (&Settings, 0, sizeof (Settings));
	ResetControls(); // controller button mappings

	Settings.LoadMethod = DEVICE_AUTO;
	Settings.SaveMethod = DEVICE_AUTO;
	sprintf (Settings.LoadFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ROMS].name); // Path to game files
	sprintf (Settings.SaveFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_SAVES].name); // Path to save files
	sprintf (Settings.CheatFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_CHEATS].name); // Path to cheat files
	sprintf (Settings.ScreenshotsFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_SCREENSHOTS].name); // Path to screenshots files
	sprintf (Settings.BorderFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_BORDERS].name); // Path to border files
	sprintf (Settings.CoverFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_COVERS].name); // Path to cover files
	sprintf (Settings.ArtworkFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ARTWORK].name); // Path to artwork files

	Settings.AutoLoad = true;
	Settings.AutoSave = true;
	Settings.AppendAuto = true;

	Settings.gbaZoomHor = 1.0; // GBA horizontal zoom level
	Settings.gbaZoomVert = 1.0; // GBA vertical zoom level
	Settings.gbZoomHor = 1.0; // GBA horizontal zoom level
	Settings.gbZoomVert = 1.0; // GBA vertical zoom level
	Settings.gbFixed = 0; // not fixed - use zoom level
	Settings.gbaFixed = 0; // not fixed - use zoom level
	Settings.videoMode = VIDEOMODE_AUTO;
	Settings.videoBilinearFilter = true;
	Settings.videoHardwareSoften = VIDEO_HW_SOFTEN_SHARP;
#if defined(HW_RVL) || defined(HW_DOL)
	Settings.videoUpscalingFilter = FILTER_NONE;
#else
	Settings.videoUpscalingFilter = 0;
#endif
	Settings.videoAspectRatioCorrection = SCALING_PARTIAL_STRETCH;
	Settings.WiiControls = false; // Match Wii Game

	Settings.videoXshift = 0; // horizontal video shift
	Settings.videoYshift = 0; // vertical video shift
	Settings.colorize = false; // Colorize mono gameboy games
	Settings.DynamicRecompilation = true;
	Settings.DisplayFrameRate = FRAMERATE_OFF;
	Settings.gbaFrameskip = true; // Turn auto-frameskip on for GBA games
	Settings.TurboModeEnabled = true; // Enabled by default

	Settings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
#ifdef HW_RVL
	Settings.ExitAction = EXITACTION_WII_AUTO;
#elif HW_DOL
	Settings.ExitAction = EXITACTION_GC_RETURN_TO_LOADER;
#endif
	Settings.AutoloadGame = false;
	Settings.MusicVolume = 20;
	Settings.SFXVolume = 40;
	Settings.Rumble = true;
	Settings.PreviewImage = PREVIEWIMAGE_COVER;
	
	Settings.BasicPalette = BASICPALETTE_GREEN;
	
#ifdef HW_RVL
	Settings.language = CONF_GetLanguage();

	if(Settings.language == LANG_TRAD_CHINESE)
		Settings.language = LANG_SIMP_CHINESE;
#elif HW_DOL
	Settings.language = SYS_GetLanguage() + LANG_ENGLISH;
#endif
	Settings.OffsetMinutesUTC = 0;
	Settings.GBHardware = GBHARDWARE_AUTO;
	Settings.SGBBorder = SGBBORDER_OFF;
}


/****************************************************************************
 * Save Preferences
 ***************************************************************************/
static char prefpath[MAXPATHLEN] = { 0 };

bool SavePrefs()
{
	char filepath[MAXPATHLEN];
	int datasize;
	int offset = 0;
	int device = DEVICE_AUTO;
	
	if(prefpath[0] != 0) {
		snprintf(filepath, sizeof(filepath), "%s/%s", prefpath, PREF_FILE_NAME);
		FindDevice(filepath, &device);
	}
	else if(appPath[0] != 0)
	{
		snprintf(filepath, sizeof(filepath), "%s/%s", appPath, PREF_FILE_NAME);
		strcpy(prefpath, appPath);
		FindDevice(filepath, &device);
	}
	else
	{
		autoSaveMethod();
		device = Settings.SaveMethod;

		if(!ChangeInterface(device, true)) {
			return false;
		}
		
		platform->getFileSystem()->getPath(filepath, device, APPFOLDER);
		if(!CreateDirectory(filepath)) {
			return false;
		}

		platform->getFileSystem()->getPath(filepath, device, APPFOLDER, PREF_FILE_NAME);
		platform->getFileSystem()->getPath(prefpath, device, APPFOLDER);
	}
	
	if(device == DEVICE_AUTO)
		return false;

	FixInvalidSettings();

	AllocSaveBuffer ();
	datasize = preparePrefsData ();

	offset = SaveFile(filepath, datasize, true);

	FreeSaveBuffer ();

	CancelAction();

	if (offset > 0)
	{
		if(appPath[0] == 0)
			strcpy(appPath, prefpath);
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
static bool prefLoadAttempted = false;

bool LoadPrefs()
{
	if(prefLoadAttempted) // already attempted loading
		return true;

	prefLoadAttempted = true;

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
#elif HW_DOL
	numDevices = 4;
	sprintf(filepath[0], "carda:/%s", APPFOLDER);
	sprintf(filepath[1], "cardb:/%s", APPFOLDER);
	sprintf(filepath[2], "port2:/%s", APPFOLDER);
	sprintf(filepath[3], "gcloader:/%s", APPFOLDER);
#endif

	for(int i=0; i<numDevices; i++) {
		prefFound = LoadPrefsFromMethod(filepath[i]);

		if(prefFound)
			break;
	}

	if(!prefFound) {
		return false;
	}

	FixInvalidSettings();
	ApplySettings();

#ifdef HW_RVL
	bg_music = (uint8_t * )bg_music_ogg;
	bg_music_size = bg_music_ogg_size;
	LoadBgMusic();
#endif
	return true;
}

void CreatePathWithPrefix(int device, const char* folder) {
    char fullPath[MAXPATHLEN];
    MakeFilePathForFolderPath(fullPath, device, folder);
    CreateDirectory(fullPath);
}

void CreateMissingDirectories() {
    char defaultFolder[MAXPATHLEN];

    if (Settings.SaveMethod > DEVICE_AUTO && ChangeInterface(Settings.SaveMethod, NOTSILENT)) {
        const char* savePointers[] = { Settings.SaveFolder, Settings.CheatFolder };

        for (int i = 0; i < SAVEFOLDER_LENGTH; i++) {
            const char* currentPath = savePointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(Settings.SaveMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, saveFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(Settings.SaveMethod, currentPath);
            }
        }
    }

    if (Settings.LoadMethod > DEVICE_AUTO && Settings.LoadMethod != DEVICE_DVD && ChangeInterface(Settings.LoadMethod, NOTSILENT)) {
        const char* loadPointers[] = {
            Settings.LoadFolder,
            Settings.ScreenshotsFolder,
            Settings.CoverFolder,
            Settings.ArtworkFolder,
			Settings.BorderFolder
        };

        for (int i = 0; i < LOADFOLDER_LENGTH; i++) {
            const char* currentPath = loadPointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(Settings.LoadMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, loadFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(Settings.LoadMethod, currentPath);
            }
        }
    }
}

bool SavePalettes(bool silent)
{
	char filepath[1024];
	int datasize;
	int offset = 0;

	if(prefpath[0] == 0)
		return false;

	snprintf(filepath, sizeof(filepath), "%s/%s", prefpath, PAL_FILE_NAME);

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
				strncpy(palettes[i].gameName, gameName, sizeof(palettes[i].gameName) - 1);
				palettes[i].gameName[sizeof(palettes[i].gameName) - 1] = 0;
				return;
			}
			else
			{
				return;
			}
		}

	palettes = (gamePalette *)realloc(palettes, sizeof(gamePalette)*(loadedPalettes+1));
	palettes[loadedPalettes] = pal;
	strncpy(palettes[loadedPalettes].gameName, gameName, sizeof(palettes[loadedPalettes].gameName) - 1);
	palettes[loadedPalettes].gameName[sizeof(palettes[loadedPalettes].gameName) - 1] = 0;
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

	snprintf(filepath, sizeof(filepath), "%s/%s", prefpath, PAL_FILE_NAME);
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

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

struct SEmuSettings EmuSettings;
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

	createXMLSection("File", "File EmuSettings");

	createXMLSetting("AutoLoad", "Auto Load", toStr(EmuSettings.AutoLoad));
	createXMLSetting("AutoSave", "Auto Save", toStr(EmuSettings.AutoSave));
	createXMLSetting("LoadMethod", "Load Method", toStr(EmuSettings.LoadMethod));
	createXMLSetting("SaveMethod", "Save Method", toStr(EmuSettings.SaveMethod));
	createXMLSetting("LoadFolder", "Load Folder", EmuSettings.LoadFolder);
	createXMLSetting("LastFileLoaded", "Last File Loaded", EmuSettings.LastFileLoaded);
	createXMLSetting("SaveFolder", "Save Folder", EmuSettings.SaveFolder);
	createXMLSetting("AppendAuto", "Append Auto to .SAV Files", BtoStr(EmuSettings.AppendAuto));
	createXMLSetting("CheatFolder", "Cheats Folder", EmuSettings.CheatFolder);
	createXMLSetting("ScreenshotsFolder", "Screenshots Folder", EmuSettings.ScreenshotsFolder);
	createXMLSetting("BorderFolder", "SGB Borders Folder", EmuSettings.BorderFolder);
	createXMLSetting("CoverFolder", "Covers Folder", EmuSettings.CoverFolder);
	createXMLSetting("ArtworkFolder", "Artwork Folder", EmuSettings.ArtworkFolder);

	createXMLSection("Network", "Network EmuSettings");

	createXMLSetting("smbip", "Share Computer IP", EmuSettings.smbShare.host);
	createXMLSetting("smbshare", "Share Name", EmuSettings.smbShare.share);
	createXMLSetting("smbuser", "Share Username", EmuSettings.smbShare.user);
	createXMLSetting("smbpwd", "Share Password", EmuSettings.smbShare.password);

	createXMLSection("Video", "Video EmuSettings");

	createXMLSetting("videoMode", "Output Mode", toStr(EmuSettings.videoMode));
	createXMLSetting("videoAspectRatioCorrection", "Aspect Ratio Correction", toStr(EmuSettings.videoAspectRatioCorrection));
	createXMLSetting("videoBilinearFilter", "Bilinear Filtering", BtoStr(EmuSettings.videoBilinearFilter));
	createXMLSetting("videoHardwareSoften", "Hardware Soften", toStr(EmuSettings.videoHardwareSoften));
	createXMLSetting("videoScanlines", "Scanlines", BtoStr(EmuSettings.videoScanlines));
	createXMLSetting("videoUpscalingFilter", "Upscaling Filter Method", toStr(EmuSettings.videoUpscalingFilter));
	createXMLSetting("gbaZoomHor", "GBA Horizontal Zoom Level", FtoStr(EmuSettings.gbaZoomHor));
	createXMLSetting("gbaZoomVert", "GBA Vertical Zoom Level", FtoStr(EmuSettings.gbaZoomVert));
	createXMLSetting("gbZoomHor", "GB Horizontal Zoom Level", FtoStr(EmuSettings.gbZoomHor));
	createXMLSetting("gbZoomVert", "GB Vertical Zoom Level", FtoStr(EmuSettings.gbZoomVert));
	createXMLSetting("gbFixed", "GB Fixed Pixel Ratio", toStr(EmuSettings.gbFixed));
	createXMLSetting("gbaFixed", "GBA Fixed Pixel Ratio", toStr(EmuSettings.gbaFixed));
	createXMLSetting("videoXshift", "Horizontal Video Shift", toStr(EmuSettings.videoXshift));
	createXMLSetting("videoYshift", "Vertical Video Shift", toStr(EmuSettings.videoYshift));

	createXMLSection("Menu", "Menu EmuSettings");

#ifdef HW_RVL
	createXMLSetting("wiimoteOrientation", "Wiimote Orientation", toStr(EmuSettings.wiimoteOrientation));
#endif
#if defined(HW_RVL) || defined(HW_DOL)
	createXMLSetting("ExitAction", "Exit Action", toStr(EmuSettings.ExitAction));
#endif
	createXMLSetting("MusicVolume", "Music Volume", toStr(EmuSettings.MusicVolume));
	createXMLSetting("SFXVolume", "Sound Effects Volume", toStr(EmuSettings.SFXVolume));
	createXMLSetting("Rumble", "Rumble", BtoStr(EmuSettings.Rumble));
	createXMLSetting("language", "Language", toStr(EmuSettings.language));
	createXMLSetting("PreviewImage", "Preview Image", toStr(EmuSettings.PreviewImage));

	createXMLSection("Emulation", "Emulation EmuSettings");

	createXMLSetting("DynamicRecompilation", "Dynamic Recompilation (JIT)", BtoStr(EmuSettings.DynamicRecompilation));
	createXMLSetting("gbaFrameskip", "GBA Frameskip", BtoStr(EmuSettings.gbaFrameskip));

	createXMLSetting("GBHardware", "Hardware (GB/GBC)", toStr(EmuSettings.GBHardware));
	createXMLSetting("SGBBorder", "Border (GB/GBC)", toStr(EmuSettings.SGBBorder));
	createXMLSetting("BasicPalette", "Basic Color Palette for GB", toStr(EmuSettings.BasicPalette));
	createXMLSetting("colorize", "Colorize Mono Gameboy", BtoStr(EmuSettings.colorize));

	createXMLSetting("DisplayFrameRate", "Show Framerate", toStr(EmuSettings.DisplayFrameRate));
	createXMLSetting("TurboModeEnabled", "Turbo Mode Enabled", BtoStr(EmuSettings.TurboModeEnabled));
	createXMLSetting("OffsetMinutesUTC", "Offset from UTC (minutes)", toStr(EmuSettings.OffsetMinutesUTC));

	createXMLSection("Controller", "Controller EmuSettings");

	createXMLController(btnmap[INPUT_HW_GAMECUBE], "gcpadmapping", "GameCube Pad");
	createXMLSetting("WiiControls", "Match Wii Game", BtoStr(EmuSettings.WiiControls));
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

void ApplyEmuSettings() {
	platform->getInput()->setWiimoteOrientation(EmuSettings.wiimoteOrientation);
	platform->getInput()->setRumbleEnabled(EmuSettings.Rumble);
	GuiSound::setDefaultVolume(SOUND::OGG, EmuSettings.MusicVolume);
	GuiSound::setDefaultVolume(SOUND::PCM, EmuSettings.SFXVolume);
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

	// File EmuSettings

	loadXMLSetting(&EmuSettings.AutoLoad, "AutoLoad");
	loadXMLSetting(&EmuSettings.AutoSave, "AutoSave");
	loadXMLSetting(&EmuSettings.LoadMethod, "LoadMethod");
	loadXMLSetting(&EmuSettings.SaveMethod, "SaveMethod");
	loadXMLSetting(EmuSettings.LoadFolder, "LoadFolder", sizeof(EmuSettings.LoadFolder));
	loadXMLSetting(EmuSettings.LastFileLoaded, "LastFileLoaded", sizeof(EmuSettings.LastFileLoaded));
	loadXMLSetting(EmuSettings.SaveFolder, "SaveFolder", sizeof(EmuSettings.SaveFolder));
	loadXMLSetting(&EmuSettings.AppendAuto, "AppendAuto");
	loadXMLSetting(EmuSettings.CheatFolder, "CheatFolder", sizeof(EmuSettings.CheatFolder));
	loadXMLSetting(EmuSettings.ScreenshotsFolder, "ScreenshotsFolder", sizeof(EmuSettings.ScreenshotsFolder));
	loadXMLSetting(EmuSettings.BorderFolder, "BorderFolder", sizeof(EmuSettings.BorderFolder));
	loadXMLSetting(EmuSettings.CoverFolder, "CoverFolder", sizeof(EmuSettings.CoverFolder));
	loadXMLSetting(EmuSettings.ArtworkFolder, "ArtworkFolder", sizeof(EmuSettings.ArtworkFolder));

	// Network EmuSettings

	loadXMLSetting(EmuSettings.smbShare.host, "smbip", sizeof(EmuSettings.smbShare.host));
	loadXMLSetting(EmuSettings.smbShare.share, "smbshare", sizeof(EmuSettings.smbShare.share));
	loadXMLSetting(EmuSettings.smbShare.user, "smbuser", sizeof(EmuSettings.smbShare.user));
	loadXMLSetting(EmuSettings.smbShare.password, "smbpwd", sizeof(EmuSettings.smbShare.password));

	// Video EmuSettings

	loadXMLSetting(&EmuSettings.videoMode, "videoMode");
	loadXMLSetting(&EmuSettings.videoAspectRatioCorrection, "videoAspectRatioCorrection");
	loadXMLSetting(&EmuSettings.videoBilinearFilter, "videoBilinearFilter");
	loadXMLSetting(&EmuSettings.videoHardwareSoften, "videoHardwareSoften");
	loadXMLSetting(&EmuSettings.videoUpscalingFilter, "videoUpscalingFilter");
	loadXMLSetting(&EmuSettings.videoScanlines, "videoScanlines");
	loadXMLSetting(&EmuSettings.gbaZoomHor, "gbaZoomHor");
	loadXMLSetting(&EmuSettings.gbaZoomVert, "gbaZoomVert");
	loadXMLSetting(&EmuSettings.gbZoomHor, "gbZoomHor");
	loadXMLSetting(&EmuSettings.gbZoomVert, "gbZoomVert");
	loadXMLSetting(&EmuSettings.gbaFixed, "gbaFixed");
	loadXMLSetting(&EmuSettings.gbFixed, "gbFixed");
	loadXMLSetting(&EmuSettings.videoXshift, "videoXshift");
	loadXMLSetting(&EmuSettings.videoYshift, "videoYshift");

	// Menu EmuSettings

#ifdef HW_RVL
	loadXMLSetting(&EmuSettings.wiimoteOrientation, "WiimoteOrientation");
#endif
#if defined(HW_RVL) || defined(HW_DOL)
	loadXMLSetting(&EmuSettings.ExitAction, "ExitAction");
#endif
	loadXMLSetting(&EmuSettings.MusicVolume, "MusicVolume");
	loadXMLSetting(&EmuSettings.SFXVolume, "SFXVolume");
	loadXMLSetting(&EmuSettings.Rumble, "Rumble");
	loadXMLSetting(&EmuSettings.language, "language");
	loadXMLSetting(&EmuSettings.PreviewImage, "PreviewImage");

	// Controller EmuSettings

	loadXMLController(btnmap[INPUT_HW_GAMECUBE], "gcpadmapping");
	loadXMLSetting(&EmuSettings.WiiControls, "WiiControls");
	loadXMLController(btnmap[INPUT_HW_WIIMOTE], "wmpadmapping");
	loadXMLController(btnmap[INPUT_HW_CLASSIC], "ccpadmapping");
	loadXMLController(btnmap[INPUT_HW_NUNCHUK], "ncpadmapping");
	loadXMLController(btnmap[INPUT_HW_WUPC], "wupcpadmapping");
	loadXMLController(btnmap[INPUT_HW_DRC], "drcpadmapping");

	// Emulation EmuSettings

	loadXMLSetting(&EmuSettings.DynamicRecompilation, "DynamicRecompilation");
	loadXMLSetting(&EmuSettings.gbaFrameskip, "gbaFrameskip");

	loadXMLSetting(&EmuSettings.GBHardware, "GBHardware");
	loadXMLSetting(&EmuSettings.SGBBorder, "SGBBorder");
	loadXMLSetting(&EmuSettings.colorize, "colorize");
	loadXMLSetting(&EmuSettings.BasicPalette, "BasicPalette");

	loadXMLSetting(&EmuSettings.DisplayFrameRate, "DisplayFrameRate");
	loadXMLSetting(&EmuSettings.TurboModeEnabled, "TurboModeEnabled");
	loadXMLSetting(&EmuSettings.OffsetMinutesUTC, "OffsetMinutesUTC");

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
	if(!isValidLoadDevice(EmuSettings.LoadMethod))
		EmuSettings.LoadMethod = DEVICE_AUTO;
	if(!isValidSaveDevice(EmuSettings.SaveMethod))
		EmuSettings.SaveMethod = DEVICE_AUTO;

	if(strlen(EmuSettings.smbShare.share) == 0 || strlen(EmuSettings.smbShare.host) == 0) {
		if(EmuSettings.LoadMethod == DEVICE_SMB) {
			EmuSettings.LoadMethod = DEVICE_AUTO;
		}
		if(EmuSettings.SaveMethod == DEVICE_SMB) {
			EmuSettings.SaveMethod = DEVICE_AUTO;
		}
	}

	if(!(EmuSettings.gbaZoomHor >= 0.5 && EmuSettings.gbaZoomHor <= 1.6))
		EmuSettings.gbaZoomHor = 1.0;
	if(!(EmuSettings.gbaZoomVert >= 0.5 && EmuSettings.gbaZoomVert <= 1.6))
		EmuSettings.gbaZoomVert = 1.0;
	if(!(EmuSettings.gbZoomHor >= 0.5 && EmuSettings.gbZoomHor <= 1.6))
		EmuSettings.gbZoomHor = 1.0;
	if(!(EmuSettings.gbZoomVert >= 0.5 && EmuSettings.gbZoomVert <= 1.6))
		EmuSettings.gbZoomVert = 1.0;
	if(!(EmuSettings.videoXshift > -50 && EmuSettings.videoXshift < 50))
		EmuSettings.videoXshift = 0;
	if(!(EmuSettings.videoYshift > -50 && EmuSettings.videoYshift < 50))
		EmuSettings.videoYshift = 0;
	if(!(EmuSettings.MusicVolume >= 0 && EmuSettings.MusicVolume <= 100))
		EmuSettings.MusicVolume = 20;
	if(!(EmuSettings.SFXVolume >= 0 && EmuSettings.SFXVolume <= 100))
		EmuSettings.SFXVolume = 40;
	if(EmuSettings.language < LANG_JAPANESE || EmuSettings.language >= LANG_LENGTH)
		EmuSettings.language = LANG_ENGLISH;
	if(!(EmuSettings.videoHardwareSoften >= VIDEO_HW_SOFTEN_OFF && EmuSettings.videoHardwareSoften < VIDEO_HW_SOFTEN_LENGTH))
		EmuSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_AUTO;
#if defined(HW_RVL) || defined(HW_DOL)
	if(!(EmuSettings.videoUpscalingFilter >= FILTER_NONE && EmuSettings.videoUpscalingFilter <= NUM_FILTERS))
		EmuSettings.videoUpscalingFilter = FILTER_NONE;
#endif
	if(!(EmuSettings.videoAspectRatioCorrection >= SCALING_MAINTAIN_ASPECT && EmuSettings.videoAspectRatioCorrection < SCALING_LENGTH))
		EmuSettings.videoAspectRatioCorrection = SCALING_MAINTAIN_ASPECT;
	if(!(EmuSettings.videoMode >= VIDEOMODE_AUTO && EmuSettings.videoMode < VIDEOMODE_LENGTH))
		EmuSettings.videoMode = VIDEOMODE_AUTO;
	if(!(EmuSettings.DisplayFrameRate >= FRAMERATE_OFF && EmuSettings.DisplayFrameRate < FRAMERATE_LENGTH))
		EmuSettings.DisplayFrameRate = FRAMERATE_OFF;
	if(!(EmuSettings.wiimoteOrientation >= WIIMOTE_ORIENTATION_AUTO && EmuSettings.wiimoteOrientation < WIIMOTE_ORIENTATION_LENGTH))
		EmuSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
}

/****************************************************************************
 * DefaultEmuSettings
 *
 * Sets all the defaults!
 ***************************************************************************/
void DefaultEmuSettings()
{
	memset (&EmuSettings, 0, sizeof (EmuSettings));
	ResetControls(); // controller button mappings

	EmuSettings.LoadMethod = DEVICE_AUTO;
	EmuSettings.SaveMethod = DEVICE_AUTO;
	sprintf (EmuSettings.LoadFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ROMS].name); // Path to game files
	sprintf (EmuSettings.SaveFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_SAVES].name); // Path to save files
	sprintf (EmuSettings.CheatFolder, "%s/%s", APPFOLDER, saveFolder[SAVEFOLDER_CHEATS].name); // Path to cheat files
	sprintf (EmuSettings.ScreenshotsFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_SCREENSHOTS].name); // Path to screenshots files
	sprintf (EmuSettings.BorderFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_BORDERS].name); // Path to border files
	sprintf (EmuSettings.CoverFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_COVERS].name); // Path to cover files
	sprintf (EmuSettings.ArtworkFolder, "%s/%s", APPFOLDER, loadFolder[LOADFOLDER_ARTWORK].name); // Path to artwork files

	EmuSettings.AutoLoad = true;
	EmuSettings.AutoSave = true;
	EmuSettings.AppendAuto = true;

	EmuSettings.gbaZoomHor = 1.0; // GBA horizontal zoom level
	EmuSettings.gbaZoomVert = 1.0; // GBA vertical zoom level
	EmuSettings.gbZoomHor = 1.0; // GBA horizontal zoom level
	EmuSettings.gbZoomVert = 1.0; // GBA vertical zoom level
	EmuSettings.gbFixed = 0; // not fixed - use zoom level
	EmuSettings.gbaFixed = 0; // not fixed - use zoom level
	EmuSettings.videoMode = VIDEOMODE_AUTO;
	EmuSettings.videoBilinearFilter = true;
	EmuSettings.videoHardwareSoften = VIDEO_HW_SOFTEN_SHARP;
#if defined(HW_RVL) || defined(HW_DOL)
	EmuSettings.videoUpscalingFilter = FILTER_NONE;
#else
	EmuSettings.videoUpscalingFilter = 0;
#endif
	EmuSettings.videoAspectRatioCorrection = SCALING_PARTIAL_STRETCH;
	EmuSettings.WiiControls = false; // Match Wii Game

	EmuSettings.videoXshift = 0; // horizontal video shift
	EmuSettings.videoYshift = 0; // vertical video shift
	EmuSettings.colorize = false; // Colorize mono gameboy games
	EmuSettings.DynamicRecompilation = true;
	EmuSettings.DisplayFrameRate = FRAMERATE_OFF;
	EmuSettings.gbaFrameskip = true; // Turn auto-frameskip on for GBA games
	EmuSettings.TurboModeEnabled = true; // Enabled by default

	EmuSettings.wiimoteOrientation = WIIMOTE_ORIENTATION_AUTO;
#ifdef HW_RVL
	EmuSettings.ExitAction = EXITACTION_WII_AUTO;
#elif HW_DOL
	EmuSettings.ExitAction = EXITACTION_GC_RETURN_TO_LOADER;
#endif
	EmuSettings.AutoloadGame = false;
	EmuSettings.MusicVolume = 20;
	EmuSettings.SFXVolume = 40;
	EmuSettings.Rumble = true;
	EmuSettings.PreviewImage = PREVIEWIMAGE_COVER;
	
	EmuSettings.BasicPalette = BASICPALETTE_GREEN;
	
#ifdef HW_RVL
	EmuSettings.language = CONF_GetLanguage();

	if(EmuSettings.language == LANG_TRAD_CHINESE)
		EmuSettings.language = LANG_SIMP_CHINESE;
#elif HW_DOL
	EmuSettings.language = SYS_GetLanguage() + LANG_ENGLISH;
#endif
	EmuSettings.OffsetMinutesUTC = 0;
	EmuSettings.GBHardware = GBHARDWARE_AUTO;
	EmuSettings.SGBBorder = SGBBORDER_OFF;
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
		device = EmuSettings.SaveMethod;

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
	ApplyEmuSettings();

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

    if (EmuSettings.SaveMethod > DEVICE_AUTO && ChangeInterface(EmuSettings.SaveMethod, NOTSILENT)) {
        const char* savePointers[] = { EmuSettings.SaveFolder, EmuSettings.CheatFolder };

        for (int i = 0; i < SAVEFOLDER_LENGTH; i++) {
            const char* currentPath = savePointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(EmuSettings.SaveMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, saveFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(EmuSettings.SaveMethod, currentPath);
            }
        }
    }

    if (EmuSettings.LoadMethod > DEVICE_AUTO && EmuSettings.LoadMethod != DEVICE_DVD && ChangeInterface(EmuSettings.LoadMethod, NOTSILENT)) {
        const char* loadPointers[] = {
            EmuSettings.LoadFolder,
            EmuSettings.ScreenshotsFolder,
            EmuSettings.CoverFolder,
            EmuSettings.ArtworkFolder,
			EmuSettings.BorderFolder
        };

        for (int i = 0; i < LOADFOLDER_LENGTH; i++) {
            const char* currentPath = loadPointers[i];

            if (strncmp(currentPath, APPFOLDER, strlen(APPFOLDER)) == 0) {
                CreatePathWithPrefix(EmuSettings.LoadMethod, APPFOLDER);
            }

            GetDefaultFolderPath(defaultFolder, loadFolder[i].name);
            if (strcmp(currentPath, defaultFolder) == 0) {
                CreatePathWithPrefix(EmuSettings.LoadMethod, currentPath);
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

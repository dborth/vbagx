/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * preferences.cpp
 *
 * Preferences save/load to XML file
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <string.h>
#include <ogcsys.h>
#include <mxml.h>

#include "vba.h"
#include "vbaconfig.h"
#include "menu.h"
#include "memcardop.h"
#include "fileop.h"
#include "filebrowser.h"
#include "input.h"
#include "button_mapping.h"
#include "gamesettings.h"

void StopColorizing();
void gbSetPalette(u32 RRGGBB[]);

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

static mxml_node_t *mxmlFindNewElement(mxml_node_t *parent, const char *nodename, const char *attr=NULL, const char *value=NULL) {
	mxml_node_t *node = mxmlFindElement(parent, xml, nodename, attr, value, MXML_DESCEND);
	if (!node) {
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

static void createXMLController(unsigned int controller[], const char * name, const char * description)
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
preparePrefsData (int method)
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
	createXMLSetting("SaveFolder", "Save Folder", GCSettings.SaveFolder);
	//createXMLSetting("CheatFolder", "Cheats Folder", GCSettings.CheatFolder);
	createXMLSetting("VerifySaves", "Verify Memory Card Saves", toStr(GCSettings.VerifySaves));

	createXMLSection("Network", "Network Settings");

	createXMLSetting("smbip", "Share Computer IP", GCSettings.smbip);
	createXMLSetting("smbshare", "Share Name", GCSettings.smbshare);
	createXMLSetting("smbuser", "Share Username", GCSettings.smbuser);
	createXMLSetting("smbpwd", "Share Password", GCSettings.smbpwd);

	createXMLSection("Video", "Video Settings");

	createXMLSetting("videomode", "Video Mode", toStr(GCSettings.videomode));
	createXMLSetting("ZoomLevel", "Zoom Level", FtoStr(GCSettings.ZoomLevel));
	createXMLSetting("render", "Video Filtering", toStr(GCSettings.render));
	createXMLSetting("scaling", "Aspect Ratio Correction", toStr(GCSettings.scaling));
	createXMLSetting("xshift", "Horizontal Video Shift", toStr(GCSettings.xshift));
	createXMLSetting("yshift", "Vertical Video Shift", toStr(GCSettings.yshift));
	createXMLSetting("colorize", "Colorize Mono Gameboy", toStr(GCSettings.colorize));

	createXMLSection("Menu", "Menu Settings");

	createXMLSetting("WiimoteOrientation", "Wiimote Orientation", toStr(GCSettings.WiimoteOrientation));
	createXMLSetting("ExitAction", "Exit Action", toStr(GCSettings.ExitAction));
	createXMLSetting("MusicVolume", "Music Volume", toStr(GCSettings.MusicVolume));
	createXMLSetting("SFXVolume", "Sound Effects Volume", toStr(GCSettings.SFXVolume));

	createXMLSection("Controller", "Controller Settings");

	createXMLSetting("WiiControls", "Match Wii Game", toStr(GCSettings.WiiControls));
	createXMLController(btnmap[CTRLR_GCPAD], "gcpadmap", "GameCube Pad");
	createXMLController(btnmap[CTRLR_WIIMOTE], "wmpadmap", "Wiimote");
	createXMLController(btnmap[CTRLR_CLASSIC], "ccpadmap", "Classic Controller");
	createXMLController(btnmap[CTRLR_NUNCHUK], "ncpadmap", "Nunchuk");
	createXMLController(btnmap[CTRLR_KEYBOARD], "kbpadmap", "Keyboard");

	int datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, XMLSaveCallback);

	mxmlDelete(xml);

	return datasize;
}

static void createXMLPalette(gamePalette *p, bool overwrite, const char *newname = NULL)
{
	if (!newname) newname = p->gameName;
	section = mxmlFindElement(xml, xml, "game", "name", newname, MXML_DESCEND);
	if (section && !overwrite) {
		return;
	} else if (!section) {
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
preparePalData (int method)
{
	xml = mxmlNewXML("1.0");
	mxmlSetWrapMargin(0); // disable line wrapping

	data = mxmlNewElement(xml, "palette");
	mxmlElementSetAttr(data, "app", APPNAME);
	mxmlElementSetAttr(data, "version", APPVERSION);
	for (int i=0; i<gamePalettesCount; i++) {
		createXMLPalette(&gamePalettes[i], false);
	}

	int datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, XMLSavePalCallback);

	mxmlDelete(xml);

	return datasize;
}

static int
prepareExistingPalData (int method, const char *gameName)
{
	int datasize = 0;
	xml = mxmlLoadString(NULL, (char *)savebuffer, MXML_TEXT_CALLBACK);

	if(xml)
	{
		// check settings version
		// we don't do anything with the version #, but we'll store it anyway
		data = mxmlFindElement(xml, xml, "palette", "version", NULL, MXML_DESCEND);
		if(data) // a version entry exists
		{
			mxmlElementSetAttr(data, "app", APPNAME);
			mxmlElementSetAttr(data, "version", APPVERSION);
		} else {
			data = mxmlNewElement(xml, "palette");
			mxmlElementSetAttr(data, "app", APPNAME);
			mxmlElementSetAttr(data, "version", APPVERSION);
		}
		for (int i=0; i<gamePalettesCount; i++) {
			createXMLPalette(&gamePalettes[i], false);
		}
		createXMLPalette(&CurrentPalette, true, gameName);

		datasize = mxmlSaveString(xml, (char *)savebuffer, SAVEBUFFERSIZE, XMLSavePalCallback);
		mxmlDelete(xml);
	}
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

static void loadXMLController(unsigned int controller[], const char * name)
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

/****************************************************************************
 * decodePrefsData
 *
 * Decodes preferences - parses XML and loads preferences into the variables
 ***************************************************************************/

static bool
decodePrefsData (int method)
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
				int curMajor = APPVERSION[0] - '0';
				int curMinor = APPVERSION[2] - '0';
				int curPoint = APPVERSION[4] - '0';

				// first we'll check that the versioning is valid
				if(!(verMajor >= 0 && verMajor <= 9 &&
					verMinor >= 0 && verMinor <= 9 &&
					verPoint >= 0 && verPoint <= 9))
					result = false;
				else if(verMajor < 2) // less than version 2.0.0
					result = false; // reset settings (sorry, should update settings instead)
				else if(verMajor > curMajor || verMinor > curMinor || verPoint > curPoint) // some future version
					result = false; // reset settings
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
			loadXMLSetting(GCSettings.SaveFolder, "SaveFolder", sizeof(GCSettings.SaveFolder));
			//loadXMLSetting(GCSettings.CheatFolder, "CheatFolder", sizeof(GCSettings.CheatFolder));
			loadXMLSetting(&GCSettings.VerifySaves, "VerifySaves");

			// Network Settings

			loadXMLSetting(GCSettings.smbip, "smbip", sizeof(GCSettings.smbip));
			loadXMLSetting(GCSettings.smbshare, "smbshare", sizeof(GCSettings.smbshare));
			loadXMLSetting(GCSettings.smbuser, "smbuser", sizeof(GCSettings.smbuser));
			loadXMLSetting(GCSettings.smbpwd, "smbpwd", sizeof(GCSettings.smbpwd));

			// Video Settings

			loadXMLSetting(&GCSettings.videomode, "videomode");
			loadXMLSetting(&GCSettings.ZoomLevel, "ZoomLevel");
			loadXMLSetting(&GCSettings.render, "render");
			loadXMLSetting(&GCSettings.scaling, "scaling");
			loadXMLSetting(&GCSettings.xshift, "xshift");
			loadXMLSetting(&GCSettings.yshift, "yshift");
			loadXMLSetting(&GCSettings.colorize, "colorize");

			// Menu Settings

			loadXMLSetting(&GCSettings.WiimoteOrientation, "WiimoteOrientation");
			loadXMLSetting(&GCSettings.ExitAction, "ExitAction");
			loadXMLSetting(&GCSettings.MusicVolume, "MusicVolume");
			loadXMLSetting(&GCSettings.SFXVolume, "SFXVolume");

			// Controller Settings

			loadXMLSetting(&GCSettings.WiiControls, "WiiControls");
			loadXMLController(btnmap[CTRLR_GCPAD], "gcpadmap");
			loadXMLController(btnmap[CTRLR_WIIMOTE], "wmpadmap");
			loadXMLController(btnmap[CTRLR_CLASSIC], "ccpadmap");
			loadXMLController(btnmap[CTRLR_NUNCHUK], "ncpadmap");
			loadXMLController(btnmap[CTRLR_KEYBOARD], "kbpadmap");
		}
		mxmlDelete(xml);
	}
	return result;
}

static bool
decodePalData (int method, const char *gameName)
{
	bool result = false;

	xml = mxmlLoadString(NULL, (char *)savebuffer, MXML_TEXT_CALLBACK);

	if(xml)
	{
		// check settings version
		// we don't do anything with the version #, but we'll store it anyway
		item = mxmlFindElement(xml, xml, "palette", "version", NULL, MXML_DESCEND);
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
				int curMajor = APPVERSION[0] - '0';
				int curMinor = APPVERSION[2] - '0';
				int curPoint = APPVERSION[4] - '0';

				// first we'll check that the versioning is valid
				if(!(verMajor >= 0 && verMajor <= 9 &&
					verMinor >= 0 && verMinor <= 9 &&
					verPoint >= 0 && verPoint <= 9))
					result = false;
				else if(verMajor < 2) // less than version 2.0.0
					result = false; // reset settings (sorry, should update settings instead)
				else if(verMajor > curMajor || verMinor > curMinor || verPoint > curPoint) // some future version
					result = true; // don't reset settings!
				else
					result = true;
			}
		}

		if(result)
		{
			// load palette for this game
			section = mxmlFindElement(xml, xml, "game", "name", gameName, MXML_DESCEND);
			// if not found, load default palette
			if (!section) section = mxmlFindElement(xml, xml, "game", "name", "default", MXML_DESCEND);
			// we have either the XML default palette or the XML game palette
			if (section) {
				item = mxmlFindElement(section, xml, "bkgr", NULL, NULL, MXML_DESCEND);
				if(item) {
					const char * tmp = mxmlElementGetAttr(item, "c0");
					if(tmp) CurrentPalette.palette[0] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c1");
					if(tmp) CurrentPalette.palette[1] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c2");
					if(tmp) CurrentPalette.palette[2] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c3");
					if(tmp) CurrentPalette.palette[3] = strtoul(tmp, NULL, 16);
				}
				item = mxmlFindElement(section, xml, "wind", NULL, NULL, MXML_DESCEND);
				if(item) {
					const char * tmp = mxmlElementGetAttr(item, "c0");
					if(tmp) CurrentPalette.palette[4] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c1");
					if(tmp) CurrentPalette.palette[5] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c2");
					if(tmp) CurrentPalette.palette[6] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c3");
					if(tmp) CurrentPalette.palette[7] = strtoul(tmp, NULL, 16);
				}
				item = mxmlFindElement(section, xml, "obj0", NULL, NULL, MXML_DESCEND);
				if(item) {
					const char * tmp = mxmlElementGetAttr(item, "c0");
					if(tmp) CurrentPalette.palette[8] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c1");
					if(tmp) CurrentPalette.palette[9] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c2");
					if(tmp) CurrentPalette.palette[10] = strtoul(tmp, NULL, 16);
				}
				item = mxmlFindElement(section, xml, "obj1", NULL, NULL, MXML_DESCEND);
				if(item) {
					const char * tmp = mxmlElementGetAttr(item, "c0");
					if(tmp) CurrentPalette.palette[11] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c1");
					if(tmp) CurrentPalette.palette[12] = strtoul(tmp, NULL, 16);
					tmp = mxmlElementGetAttr(item, "c2");
					if(tmp) CurrentPalette.palette[13] = strtoul(tmp, NULL, 16);
				}				
				const char *use = mxmlElementGetAttr(section, "use");
				if (use) {
					if (atoi(use)==0) StopColorizing();
					else gbSetPalette(CurrentPalette.palette);
				} else {
					gbSetPalette(CurrentPalette.palette);
				}
			// there is no palette for this game and no default in XML
			} else {
				// use the hardcoded default palette
				CurrentPalette = gamePalettes[0];
				gbSetPalette(CurrentPalette.palette);
			}
			
		}
		mxmlDelete(xml);
	}
	return result;
}

/****************************************************************************
 * Save Preferences
 ***************************************************************************/
bool
SavePrefs (bool silent)
{
	char filepath[1024];
	int datasize;
	int offset = 0;

	// We'll save using the first available method (probably SD) since this
	// is the method preferences will be loaded from by default
	int method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	if(!MakeFilePath(filepath, FILE_PREF, method))
		return false;

	if (!silent)
		ShowAction ("Saving preferences...");

	FixInvalidSettings();

	AllocSaveBuffer ();
	datasize = preparePrefsData (method);

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		// Set the comments
		char prefscomment[2][32];
		memset(prefscomment, 0, 64);
		sprintf (prefscomment[0], "%s Prefs", APPNAME);
		sprintf (prefscomment[1], "Preferences");
		SetMCSaveComments(prefscomment);
	}

	offset = SaveFile(filepath, datasize, method, silent);

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

bool
CreateAndLoadPalette(bool silent, const char *gameName)
{
	// Load palette from hardcoded palettes
	int snum = -1;
	for(int i=1; i < gamePalettesCount; i++) {
		if(strcmp(gameName, gamePalettes[i].gameName)==0)
		{
			snum = i;
			break;
		}
	}
	// match found!
	if(snum >= 0)
	{
		CurrentPalette = gamePalettes[snum];
	} else {
		CurrentPalette = gamePalettes[0];
	}
	gbSetPalette(CurrentPalette.palette);
	
	// Now create the XML palette file
	char filepath[1024];
	int datasize;
	int offset = 0;

	// We'll save using the first available method (probably SD) since this
	// is the method preferences will be loaded from by default
	int method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	if(!MakeFilePath(filepath, FILE_PAL, method))
		return false;

	if (!silent)
		ShowAction ("Saving palette...");

	AllocSaveBuffer ();
	datasize = preparePalData (method);

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		// Set the comments
		char prefscomment[2][32];
		memset(prefscomment, 0, 64);
		sprintf (prefscomment[0], "%s Pal", APPNAME);
		sprintf (prefscomment[1], "Palette");
		SetMCSaveComments(prefscomment);
	}

	offset = SaveFile(filepath, datasize, method, silent);

	FreeSaveBuffer ();

	CancelAction();

	if (offset > 0)
	{
		if (!silent)
			InfoPrompt("Palette saved");
		return true;
	}
	return false;
}

bool
SavePaletteMethod(bool silent, int method, const char *gameName)
{
	char filepath[1024];
	int offset = 0;

	if(!MakeFilePath(filepath, FILE_PAL, method))
		return false;

	AllocSaveBuffer ();

	offset = LoadFile(filepath, method, SILENT);

	int datasize = 0;
	if (offset > 0)
		datasize = prepareExistingPalData (method, gameName);
	if (!datasize) {
		FreeSaveBuffer();
		CancelAction();		
		return false;
	}


	if (!silent)
		ShowAction ("Saving palette...");

	// We'll save using the first available method (probably SD) since this
	// is the method preferences will be loaded from by default
	method = autoSaveMethod(silent);

	if(method == METHOD_AUTO)
		return false;

	if(!MakeFilePath(filepath, FILE_PREF, method))
		return false;

	if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
	{
		// Set the comments
		char prefscomment[2][32];
		memset(prefscomment, 0, 64);
		sprintf (prefscomment[0], "%s Pal", APPNAME);
		sprintf (prefscomment[1], "Palette");
		SetMCSaveComments(prefscomment);
	}

	offset = SaveFile(filepath, datasize, method, silent);

	FreeSaveBuffer ();

	CancelAction();

	if (offset > 0)
	{
		if (!silent)
			InfoPrompt("Palette saved");
		return true;
	}
	return false;
}


/****************************************************************************
 * Load Preferences from specified method
 ***************************************************************************/
bool
LoadPrefsFromMethod (int method)
{
	bool retval = false;
	char filepath[1024];
	int offset = 0;

	if(!MakeFilePath(filepath, FILE_PREF, method))
		return false;

	AllocSaveBuffer ();

	offset = LoadFile(filepath, method, SILENT);

	if (offset > 0)
		retval = decodePrefsData (method);

	FreeSaveBuffer ();

	return retval;
}

bool
LoadPalFromMethod (int method, const char *gameName)
{
	bool retval = false;
	char filepath[1024];
	int offset = 0;

	if(!MakeFilePath(filepath, FILE_PAL, method))
		return false;

	AllocSaveBuffer ();

	offset = LoadFile(filepath, method, SILENT);

	if (offset > 0)
		retval = decodePalData (method, gameName);

	FreeSaveBuffer ();

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
	if(ChangeInterface(METHOD_SD, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_SD);
	if(!prefFound && ChangeInterface(METHOD_USB, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_USB);
	if(!prefFound && TestMC(CARD_SLOTA, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_MC_SLOTA);
	if(!prefFound && TestMC(CARD_SLOTB, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_MC_SLOTB);
	if(!prefFound && ChangeInterface(METHOD_SMB, SILENT))
		prefFound = LoadPrefsFromMethod(METHOD_SMB);

	prefLoaded = true; // attempted to load preferences

	if(prefFound)
		FixInvalidSettings();

	return prefFound;
}

bool LoadPalette(const char *gameName)
{
	bool prefFound = false;
	if(ChangeInterface(METHOD_SD, SILENT))
		prefFound = LoadPalFromMethod(METHOD_SD, gameName);
	if(!prefFound && ChangeInterface(METHOD_USB, SILENT))
		prefFound = LoadPalFromMethod(METHOD_USB, gameName);
	if(!prefFound && TestMC(CARD_SLOTA, SILENT))
		prefFound = LoadPalFromMethod(METHOD_MC_SLOTA, gameName);
	if(!prefFound && TestMC(CARD_SLOTB, SILENT))
		prefFound = LoadPalFromMethod(METHOD_MC_SLOTB, gameName);
	if(!prefFound && ChangeInterface(METHOD_SMB, SILENT))
		prefFound = LoadPalFromMethod(METHOD_SMB, gameName);

	if(!prefFound) {
		return CreateAndLoadPalette(SILENT, gameName);
	}
	return true;
}

bool SavePalette(bool silent, const char *gameName)
{
	bool prefFound = false;
	if(ChangeInterface(METHOD_SD, SILENT))
		prefFound = SavePaletteMethod(silent, METHOD_SD, gameName);
	if(!prefFound && ChangeInterface(METHOD_USB, SILENT))
		prefFound = SavePaletteMethod(silent, METHOD_USB, gameName);
	if(!prefFound && TestMC(CARD_SLOTA, SILENT))
		prefFound = SavePaletteMethod(silent, METHOD_MC_SLOTA, gameName);
	if(!prefFound && TestMC(CARD_SLOTB, SILENT))
		prefFound = SavePaletteMethod(silent, METHOD_MC_SLOTB, gameName);
	if(!prefFound && ChangeInterface(METHOD_SMB, SILENT))
		prefFound = SavePaletteMethod(silent, METHOD_SMB, gameName);

	if(!prefFound) {
		gamePalette pal = CurrentPalette;
		CreateAndLoadPalette(silent, gameName);
		CurrentPalette = pal;
		gbSetPalette(CurrentPalette.palette);
		SavePaletteMethod(silent, METHOD_SD, gameName);
	}
	return true;
}

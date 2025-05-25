/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
 *
 * filebrowser.cpp
 *
 * Generic file routines - reading, writing, browsing
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <wiiuse/wpad.h>
#include <sys/dir.h>
#include <malloc.h>

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "vbagx.h"
#include "vbasupport.h"
#include "vmmem.h"
#include "filebrowser.h"
#include "menu.h"
#include "video.h"
#include "networkop.h"
#include "fileop.h"
#include "input.h"
#include "gcunzip.h"

extern "C" {
extern char* strcasestr(const char *, const char *);
}

BROWSERINFO browser;
BROWSERENTRY * browserList = NULL; // list of files/folders in browser

char szpath[MAXPATHLEN];
char szname[MAXPATHLEN];
bool inSz = false;

char ROMFilename[512];
bool ROMLoaded = false;
bool loadingFile = false;

/****************************************************************************
* autoLoadMethod()
* Auto-determines and sets the load device
* Returns device set
****************************************************************************/
int autoLoadMethod()
{
	ShowAction ("Attempting to determine load device...");

	int device = DEVICE_AUTO;

	if(ChangeInterface(DEVICE_SD, SILENT))
		device = DEVICE_SD;
	else if(ChangeInterface(DEVICE_USB, SILENT))
		device = DEVICE_USB;
	else if(ChangeInterface(DEVICE_SD_SLOTA, SILENT))
		device = DEVICE_SD_SLOTA;
	else if(ChangeInterface(DEVICE_SD_SLOTB, SILENT))
		device = DEVICE_SD_SLOTB;
	else if(ChangeInterface(DEVICE_SD_PORT2, SILENT))
		device = DEVICE_SD_PORT2;
	else if(ChangeInterface(DEVICE_SD_GCLOADER, SILENT))
		device = DEVICE_SD_GCLOADER;
	else if(ChangeInterface(DEVICE_DVD, SILENT))
		device = DEVICE_DVD;
	else if(ChangeInterface(DEVICE_SMB, SILENT))
		device = DEVICE_SMB;

	if(GCSettings.LoadMethod == DEVICE_AUTO)
		GCSettings.LoadMethod = device; // save device found for later use
	CancelAction();
	return device;
}

/****************************************************************************
* autoSaveMethod()
* Auto-determines and sets the save device
* Returns device set
****************************************************************************/
int autoSaveMethod(bool silent)
{
	if(!silent)
		ShowAction ("Attempting to determine save device...");

	int device = DEVICE_AUTO;

	if(ChangeInterface(DEVICE_SD, SILENT))
		device = DEVICE_SD;
	else if(ChangeInterface(DEVICE_USB, SILENT))
		device = DEVICE_USB;
	else if(ChangeInterface(DEVICE_SD_SLOTA, SILENT))
		device = DEVICE_SD_SLOTA;
	else if(ChangeInterface(DEVICE_SD_SLOTB, SILENT))
		device = DEVICE_SD_SLOTB;
	else if(ChangeInterface(DEVICE_SD_PORT2, SILENT))
		device = DEVICE_SD_PORT2;
	else if(ChangeInterface(DEVICE_SD_GCLOADER, SILENT))
		device = DEVICE_SD_GCLOADER;
	else if(ChangeInterface(DEVICE_SMB, SILENT))
		device = DEVICE_SMB;
	else if(!silent)
		ErrorPrompt("Unable to locate a save device!");

	if(GCSettings.SaveMethod == DEVICE_AUTO)
		GCSettings.SaveMethod = device; // save device found for later use

	CancelAction();
	return device;
}

/****************************************************************************
 * ResetBrowser()
 * Clears the file browser memory, and allocates one initial entry
 ***************************************************************************/
void ResetBrowser()
{
	browser.numEntries = 0;
	browser.selIndex = 0;
	browser.pageIndex = 0;
	browser.size = 0;
}

bool AddBrowserEntry()
{
	if(browser.size >= MAX_BROWSER_SIZE)
	{
		ErrorPrompt("Out of memory: too many files!");
		return false; // out of space
	}

	memset(&(browserList[browser.size]), 0, sizeof(BROWSERENTRY)); // clear the new entry
	browser.size++;
	return true;
}

/****************************************************************************
 * CleanupPath()
 * Cleans up the filepath, removing double // and replacing \ with /
 ***************************************************************************/
static void CleanupPath(char * path)
{
	if(!path || path[0] == 0)
		return;
	
	int pathlen = strlen(path);
	int j = 0;
	for(int i=0; i < pathlen && i < MAXPATHLEN; i++)
	{
		if(path[i] == '\\')
			path[i] = '/';

		if(j == 0 || !(path[j-1] == '/' && path[i] == '/'))
			path[j++] = path[i];
	}
	path[j] = 0;
}

bool IsDeviceRoot(char * path)
{
	if(path == NULL || path[0] == 0)
		return false;

	if( strcmp(path, "sd:/")    == 0 ||
		strcmp(path, "usb:/")   == 0 ||
		strcmp(path, "dvd:/")   == 0 ||
		strcmp(path, "smb:/")   == 0 ||
		strcmp(path, "carda:/") == 0 ||
		strcmp(path, "cardb:/") == 0 ||
		strcmp(path, "port2:/") == 0 ||
		strcmp(path, "gcloader:/") == 0 )
	{
		return true;
	}
	return false;
}

/****************************************************************************
 * UpdateDirName()
 * Update curent directory name for file browser
 ***************************************************************************/
int UpdateDirName()
{
	int size=0;
	char * test;
	char temp[1024];
	int device = 0;

	if(browser.numEntries == 0)
		return 1;

	FindDevice(browser.dir, &device);

	/* current directory doesn't change */
	if (strcmp(browserList[browser.selIndex].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(browserList[browser.selIndex].filename,"..") == 0)
	{
		// already at the top level
		if(IsDeviceRoot(browser.dir))
		{
			browser.dir[0] = 0; // remove device - we are going to the device listing screen
		}
		else
		{
			/* determine last subdirectory namelength */
			sprintf(temp,"%s",browser.dir);
			test = strtok(temp,"/");
			while (test != NULL)
			{
				size = strlen(test);
				test = strtok(NULL,"/");
			}
	
			/* remove last subdirectory name */
			size = strlen(browser.dir) - size - 1;
			strncpy(GCSettings.LastFileLoaded, &browser.dir[size], strlen(browser.dir) - size - 1); //set as loaded file the previous dir
			GCSettings.LastFileLoaded[strlen(browser.dir) - size - 1] = 0;
			browser.dir[size] = 0;
		}

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(browser.dir+strlen(browser.dir), "%s/", browserList[browser.selIndex].filename);
			return 1;
		}
		else
		{
			ErrorPrompt("Directory name is too long!");
			return -1;
		}
	}
}

bool MakeFilePath(char filepath[], int type, char * filename, int filenum)
{
	char file[512];
	char folder[1024];
	char ext[4];
	char temppath[MAXPATHLEN];

	if(type == FILE_ROM)
	{
		// Check path length
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) >= MAXPATHLEN)
		{
			ErrorPrompt("Maximum filepath length reached!");
			filepath[0] = 0;
			return false;
		}
		else
		{
			sprintf(temppath, "%s%s",browser.dir,browserList[browser.selIndex].filename);
		}
	}
	else if (type == FILE_BORDER_PNG)
	{
		const char* loadedpath = filename;
		if (loadedpath == NULL) loadedpath = "default";
		// Ensure that loadedname contains only the filename, not the path
		const char* loadedname = strrchr(loadedpath, '/');
		if (loadedname == NULL) loadedname = loadedpath;
		
		// Check path length
		if ((strlen(pathPrefix[GCSettings.LoadMethod]) + strlen(GCSettings.BorderFolder) + strlen(loadedname)) >= MAXPATHLEN) {
			ErrorPrompt("Maximum filepath length reached!");
			filepath[0] = 0;
			return false;
		}
		
		StripExt(file, loadedname);
		sprintf(temppath, "%s%s/%s.png", pathPrefix[GCSettings.LoadMethod], GCSettings.BorderFolder, file);
	}
	else
	{
		if(GCSettings.SaveMethod == DEVICE_AUTO)
			GCSettings.SaveMethod = autoSaveMethod(SILENT);

		if(GCSettings.SaveMethod == DEVICE_AUTO)
			return false;

		switch(type)
		{
			case FILE_SRAM:
			case FILE_SNAPSHOT:
				sprintf(folder, GCSettings.SaveFolder);

				if(type == FILE_SRAM) sprintf(ext, "sav");
				else sprintf(ext, "sgm");

				if(filenum >= -1)
				{
					if(filenum == -1)
						sprintf(file, "%s.%s", filename, ext);
					else if(filenum == 0)
						if (GCSettings.AppendAuto <= 0)
						{
							sprintf(file, "%s.%s", filename, ext);
						}
						else
						{
							sprintf(file, "%s Auto.%s", filename, ext);
						}
					else
						sprintf(file, "%s %i.%s", filename, filenum, ext);
				}
				else
				{
					sprintf(file, "%s", filename);
				}
				break;
		}
		sprintf (temppath, "%s%s/%s", pathPrefix[GCSettings.SaveMethod], folder, file);
	}
	CleanupPath(temppath); // cleanup path
	snprintf(filepath, MAXPATHLEN, "%s", temppath);
	return true;
}

/****************************************************************************
 * FileSortCallback
 *
 * Quick sort callback to sort file entries with the following order:
 *   .
 *   ..
 *   <dirs>
 *   <files>
 ***************************************************************************/
int FileSortCallback(const void *f1, const void *f2)
{
	/* Special case for implicit directories */
	if(((BROWSERENTRY *)f1)->filename[0] == '.' || ((BROWSERENTRY *)f2)->filename[0] == '.')
	{
		if(strcmp(((BROWSERENTRY *)f1)->filename, ".") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, ".") == 0) { return 1; }
		if(strcmp(((BROWSERENTRY *)f1)->filename, "..") == 0) { return -1; }
		if(strcmp(((BROWSERENTRY *)f2)->filename, "..") == 0) { return 1; }
	}

	/* If one is a file and one is a directory the directory is first. */
	if(((BROWSERENTRY *)f1)->isdir && !(((BROWSERENTRY *)f2)->isdir)) return -1;
	if(!(((BROWSERENTRY *)f1)->isdir) && ((BROWSERENTRY *)f2)->isdir) return 1;

	return strcasecmp(((BROWSERENTRY *)f1)->filename, ((BROWSERENTRY *)f2)->filename);
}

/****************************************************************************
 * IsSz
 *
 * Checks if the specified file is a 7z
 ***************************************************************************/
bool IsSz()
{
	if (strlen(browserList[browser.selIndex].filename) > 4)
	{
		char * p = strrchr(browserList[browser.selIndex].filename, '.');

		if (p != NULL)
			if(strcasecmp(p, ".7z") == 0)
				return true;
	}
	return false;
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
 ***************************************************************************/
void StripExt(char* returnstring, const char * inputstring)
{
	char* loc_dot;

	snprintf(returnstring, MAXJOLIET, "%s", inputstring);

	if(inputstring == NULL || strlen(inputstring) < 4)
		return;

	loc_dot = strrchr(returnstring,'.');
	if (loc_dot != NULL)
		*loc_dot = 0; // strip file extension
}

// Shorten a ROM filename by removing the extension, URLs, id numbers and other rubbish
void ShortenFilename(char * returnstring, char * inputstring)
{
	if (!inputstring) {
		returnstring[0] = '\0';
		return;
	}
	// skip URLs in brackets
	char * dotcom = (char *) strstr(inputstring, ".com)");
	char * url = NULL;
	if (dotcom) {
		url = (char *) strchr(inputstring, '(');
		if (url >= dotcom) {
			url = NULL;
			dotcom = NULL;
		} else dotcom+= 5; // point to after ')'
	}
	// skip URLs not in brackets
	if (!dotcom) {
		dotcom = (char *) strstr(inputstring, ".com");
		url = NULL;
		if (dotcom) {
			url = (char *) strstr(inputstring, "www");
			if (url >= dotcom) {
				url = NULL;
				dotcom = NULL;
			} else dotcom+= 4; // point to after ')'
		}
	}
	// skip file extension
	char * loc_dot = (char *)strrchr(inputstring,'.');
	char * s = (char *)inputstring;
	char * r = (char *)returnstring;
	// skip initial whitespace, numbers, - and _ ...
	while ((*s!='\0' && *s<=' ') || *s=='-' || *s=='_' || *s=='+') s++;
	// ... except those that SHOULD begin with numbers
	if (strncmp(s,"3D",2)==0) for (int i=0; i<2; i++, r++, s++) *r=*s;
	if (strncmp(s,"1st",3)==0 || strncmp(s,"2nd",3)==0 || strncmp(s,"3rd",3)==0 || strncmp(s,"4th",3)==0) for (int i=0; i<3; i++, r++, s++) *r=*s;
	if (strncmp(s,"199",3)==0 || strncmp(s,"007",3)==0 || strncmp(s,"4x4",3)==0 || strncmp(s,"720",3)==0 || strncmp(s,"10 ",3)==0) for (int i=0; i<3; i++, r++, s++) *r=*s;
	if (strncmp(s,"102 ",4)==0 || strncmp(s,"1942",4)==0 || strncmp(s,"3 Ch",4)==0) for (int i=0; i<4; i++, r++, s++) *r=*s;
	if (strncmp(s,"2 in 1",6)==0 || strncmp(s,"3 in 1",6)==0 || strncmp(s,"4 in 1",6)==0) for (int i=0; i<6; i++, r++, s++) *r=*s;
	if (strncmp(s,"2-in-1",6)==0 || strncmp(s,"3-in-1",6)==0 || strncmp(s,"4-in-1",6)==0) for (int i=0; i<6; i++, r++, s++) *r=*s;
	while (*s>='0' && *s<='9') s++;
	if (r==(char *)returnstring) while ((*s!='\0' && *s<=' ') || *s=='-' || *s=='_' || *s=='+') s++;
	// now go through rest of string until we get to the end or the extension
	while (*s!='\0' && (loc_dot==NULL || s<loc_dot)) {
		// skip url
		if (s==url) s=dotcom;
		// skip whitespace, numbers, - and _ after url
		if (s==dotcom) {
			while ((*s>'\0' && *s<=' ') || *s=='-' || *s=='_') s++;
			while (*s>='0' && *s<='9') s++;
			while ((*s>'\0' && *s<=' ') || *s=='-' || *s=='_') s++;
		}
		// skip all but 1 '-', '_' or space in a row
		char c = s[0];
		if (c==s[1] && (c=='-' || c=='_' || c==' ')) s++;
		// skip space before hyphen
		if (*s==' ' && s[1]=='-') s++;
		// copy character to result
		if (*s=='_') *r=' ';
		else *r = *s;
		// skip spaces after hyphen
		if (*s=='-') while (s[1]==' ') s++;
		s++; r++;
	}
	*r = '\0';
	// if the result is too short, abandon what we did and just strip the ext instead
	if (strlen(returnstring) <= 4) StripExt(returnstring, inputstring);
}

/****************************************************************************
 * BrowserLoadSz
 *
 * Opens the selected 7z file, and parses a listing of the files within
 ***************************************************************************/
int BrowserLoadSz()
{
	memset(szpath, 0, MAXPATHLEN);
	strncpy(szpath, browser.dir, strlen(browser.dir) - 1);
	
	strncpy(szname, strrchr(szpath, '/') + 1, strrchr(szpath, '.') - strrchr(szpath, '/'));
	*strrchr(szname, '.') = '\0';

	int szfiles = SzParse(szpath);
	if(szfiles)
	{
		browser.numEntries = szfiles;
		inSz = true;
	}
	else
		ErrorPrompt("Error opening archive!");

	return szfiles;
}

/****************************************************************************
 * BrowserLoadFile
 *
 * Loads the selected ROM
 ***************************************************************************/
int BrowserLoadFile()
{
	int device;

	if(!FindDevice(browser.dir, &device))
		return 0;

	// store the filename (w/o ext) - used for sram/freeze naming
	StripExt(ROMFilename, browserList[browser.selIndex].filename);
	snprintf(GCSettings.LastFileLoaded, MAXPATHLEN, "%s", browserList[browser.selIndex].filename);

	loadingFile = true;
	ROMLoaded = LoadVBAROM();
	loadingFile = false;

	if (!ROMLoaded)
	{
		if(inSz)
		{
			browser.selIndex = 0;
			BrowserChangeFolder();
		}
		ErrorPrompt("Error loading game!");
	}
	else
	{
		if (GCSettings.AutoLoad == 1)
			LoadBatteryOrStateAuto(FILE_SRAM, SILENT);
		else if (GCSettings.AutoLoad == 2)
			LoadBatteryOrStateAuto(FILE_SNAPSHOT, SILENT);

		ResetBrowser();
	}
	CancelAction();
	return ROMLoaded;
}

/****************************************************************************
 * BrowserChangeFolder
 *
 * Update current directory and set new entry list if directory has changed
 ***************************************************************************/
int BrowserChangeFolder()
{
	int device = 0;
	FindDevice(browser.dir, &device);
	
	if(inSz && browser.selIndex == 0) // inside a 7z, requesting to leave
	{
		inSz = false;
		SzClose();
	}

	if(!UpdateDirName()) 
		return -1;

	HaltParseThread();
	CleanupPath(browser.dir);
	ResetBrowser();

	if(browser.dir[0] != 0)
	{
		if(strstr(browser.dir, ".7z"))
		{
			BrowserLoadSz();
		}
		else 
		{
			ParseDirectory(true, true);
		}
		FindAndSelectLastLoadedFile();
	}

	if(browser.numEntries == 0)
	{
		browser.dir[0] = 0;
		int i=0;
		
#ifdef HW_RVL
		AddBrowserEntry();
		sprintf(browserList[i].filename, "sd:/");
		sprintf(browserList[i].displayname, "SD Card");
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = ICON_SD;
		i++;

		AddBrowserEntry();
		sprintf(browserList[i].filename, "usb:/");
		sprintf(browserList[i].displayname, "USB Mass Storage");
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = ICON_USB;
		i++;
#else
		AddBrowserEntry();
		sprintf(browserList[i].filename, "carda:/");
		sprintf(browserList[i].displayname, "SD Gecko Slot A");
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = ICON_SD;
		i++;
		
		AddBrowserEntry();
		sprintf(browserList[i].filename, "cardb:/");
		sprintf(browserList[i].displayname, "SD Gecko Slot B");
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = ICON_SD;
		i++;

		AddBrowserEntry();
		sprintf(browserList[i].filename, "port2:/");
		sprintf(browserList[i].displayname, "SD in SP2");
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = ICON_SD;
		i++;
#endif
		AddBrowserEntry();
		sprintf(browserList[i].filename, "smb:/");
		sprintf(browserList[i].displayname, "Network Share");
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = ICON_SMB;
		i++;
		
		AddBrowserEntry();
		sprintf(browserList[i].filename, "dvd:/");
		sprintf(browserList[i].displayname, "Data DVD");
		browserList[i].length = 0;
		browserList[i].isdir = 1;
		browserList[i].icon = ICON_DVD;
		i++;
		
		browser.numEntries += i;
	}
	
	if(browser.dir[0] == 0)
	{
		GCSettings.LoadFolder[0] = 0;
		GCSettings.LoadMethod = 0;
	}
	else
	{
		char * path = StripDevice(browser.dir);
		if(path != NULL)
			strcpy(GCSettings.LoadFolder, path);
		FindDevice(browser.dir, &GCSettings.LoadMethod);
	}

	return browser.numEntries;
}

/****************************************************************************
 * OpenROM
 * Displays a list of ROMS on load device
 ***************************************************************************/
int
OpenGameList ()
{
	int device = GCSettings.LoadMethod;
	bool autoLoad = false;

	if(device == DEVICE_AUTO && strlen(GCSettings.LoadFolder) > 0) {
		device = autoLoadMethod();
		autoLoad = true;
	}

	// change current dir to roms directory
	if(device > 0) {
		sprintf(browser.dir, "%s%s/", pathPrefix[device], GCSettings.LoadFolder);

		if(autoLoad) {
			DIR *dir = opendir(browser.dir);

			if(dir == NULL) {
				sprintf(browser.dir, "%s", pathPrefix[device]);
			}
			else {
				closedir(dir);
			}
		}
	}
	else {
		browser.dir[0] = 0;
	}
	
	BrowserChangeFolder();
	return browser.numEntries;
}

bool AutoloadGame(char* filepath, char* filename) {
	ResetBrowser();

	selectLoadedFile = 1;
	std::string dir(filepath);
	dir.assign(&dir[dir.find_last_of(":") + 2]);
	strncpy(GCSettings.LoadFolder, dir.c_str(), sizeof(GCSettings.LoadFolder));
	OpenGameList();

	for(int i = 0; i < browser.numEntries; i++) {
		// Skip it
		if (strcmp(browserList[i].filename, ".") == 0 || strcmp(browserList[i].filename, "..") == 0) {
			continue;
		}
		if(strcasestr(browserList[i].filename, filename) != NULL) {
			browser.selIndex = i;
			if(IsSz()) {
				BrowserLoadSz();
				browser.selIndex = 1;
			}
			break;
		}
	}
	if(BrowserLoadFile() > 0) {
		return true;
	}
	return false;
}

/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * filebrowser.cpp
 *
 * Generic file routines - reading, writing, browsing
 ***************************************************************************/

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <sys/dir.h>
#include <malloc.h>

#ifdef HW_RVL
#include <di/di.h>
#endif

#include "vba.h"
#include "dvd.h"
#include "vbasupport.h"
#include "vmmem.h"
#include "filebrowser.h"
#include "menu.h"
#include "video.h"
#include "networkop.h"
#include "fileop.h"
#include "memcardop.h"
#include "input.h"
#include "gcunzip.h"
#include "wiiusbsupport.h"

BROWSERINFO browser;
BROWSERENTRY * browserList = NULL; // list of files/folders in browser

char rootdir[10];
char szpath[MAXPATHLEN];
bool inSz = false;

char ROMFilename[512];
bool ROMLoaded = false;

/****************************************************************************
* autoLoadMethod()
* Auto-determines and sets the load method
* Returns method set
****************************************************************************/
int autoLoadMethod()
{
	ShowAction ("Attempting to determine load method...");

	int method = METHOD_AUTO;

	if(ChangeInterface(METHOD_SD, SILENT))
		method = METHOD_SD;
	else if(ChangeInterface(METHOD_USB, SILENT))
		method = METHOD_USB;
	else if(ChangeInterface(METHOD_DVD, SILENT))
		method = METHOD_DVD;
	else if(ChangeInterface(METHOD_SMB, SILENT))
		method = METHOD_SMB;
	else
		ErrorPrompt("Unable to auto-determine load method!");

	if(GCSettings.LoadMethod == METHOD_AUTO)
		GCSettings.LoadMethod = method; // save method found for later use
	CancelAction();
	return method;
}

/****************************************************************************
* autoSaveMethod()
* Auto-determines and sets the save method
* Returns method set
****************************************************************************/
int autoSaveMethod(bool silent)
{
	if(!silent)
		ShowAction ("Attempting to determine save method...");

	int method = METHOD_AUTO;

	if(ChangeInterface(METHOD_SD, SILENT))
		method = METHOD_SD;
	else if(ChangeInterface(METHOD_USB, SILENT))
		method = METHOD_USB;
	else if(ChangeInterface(METHOD_MC_SLOTA, SILENT))
		method = METHOD_MC_SLOTA;
	else if(ChangeInterface(METHOD_MC_SLOTB, SILENT))
		method = METHOD_MC_SLOTB;
	else if(ChangeInterface(METHOD_SMB, SILENT))
		method = METHOD_SMB;
	else if(!silent)
		ErrorPrompt("Unable to auto-determine save method!");

	if(GCSettings.SaveMethod == METHOD_AUTO)
		GCSettings.SaveMethod = method; // save method found for later use

	CancelAction();
	return method;
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

	// Clear any existing values
	if(browserList != NULL)
	{
		free(browserList);
		browserList = NULL;
	}
	// set aside space for 1 entry
	browserList = (BROWSERENTRY *)malloc(sizeof(BROWSERENTRY));
	memset(browserList, 0, sizeof(BROWSERENTRY));
}

/****************************************************************************
 * CleanupPath()
 * Cleans up the filepath, removing double // and replacing \ with /
 ***************************************************************************/
static void CleanupPath(char * path)
{
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

	if(strlen(path) == 0)
		sprintf(path, "/");
}

/****************************************************************************
 * UpdateDirName()
 * Update curent directory name for file browser
 ***************************************************************************/
int UpdateDirName(int method)
{
	int size=0;
	char * test;
	char temp[1024];

	// update DVD directory
	if(method == METHOD_DVD)
		SetDVDdirectory(browserList[browser.selIndex].offset, browserList[browser.selIndex].length);

	/* current directory doesn't change */
	if (strcmp(browserList[browser.selIndex].filename,".") == 0)
	{
		return 0;
	}
	/* go up to parent directory */
	else if (strcmp(browserList[browser.selIndex].filename,"..") == 0)
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
		browser.dir[size] = 0;

		return 1;
	}
	/* Open a directory */
	else
	{
		/* test new directory namelength */
		if ((strlen(browser.dir)+1+strlen(browserList[browser.selIndex].filename)) < MAXPATHLEN)
		{
			/* update current directory name */
			sprintf(browser.dir, "%s%s/",browser.dir, browserList[browser.selIndex].filename);
			return 1;
		}
		else
		{
			ErrorPrompt("Directory name is too long!");
			return -1;
		}
	}
}

bool MakeFilePath(char filepath[], int type, int method, char * filename, int filenum)
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
	else
	{
		switch(type)
		{
			case FILE_SRAM:
			case FILE_SNAPSHOT:
				sprintf(folder, GCSettings.SaveFolder);

				if(type == FILE_SRAM) sprintf(ext, "sav");
				else sprintf(ext, "sgm");

				if(filenum >= -1)
				{
					if(method == METHOD_MC_SLOTA || method == METHOD_MC_SLOTB)
					{
						if(filenum > 9)
						{
							return false;
						}
						else if(filenum == -1)
						{
							filename[27] = 0; // truncate filename
							sprintf(file, "%s.%s", filename, ext);
						}
						else
						{
							filename[26] = 0; // truncate filename
							sprintf(file, "%s%i.%s", filename, filenum, ext);
						}
					}
					else
					{
						if(filenum == -1)
							sprintf(file, "%s.%s", filename, ext);
						else if(filenum == 0)
							sprintf(file, "%s Auto.%s", filename, ext);
						else
							sprintf(file, "%s %i.%s", filename, filenum, ext);
					}
				}
				else
				{
					sprintf(file, "%s", filename);
				}
				break;
			case FILE_CHEAT:
				sprintf(folder, GCSettings.CheatFolder);
				sprintf(file, "%s.cht", ROMFilename);
				break;
			case FILE_PREF:
				sprintf(folder, appPath);
				sprintf(file, "%s", PREF_FILE_NAME);
				break;
			case FILE_PAL:
				sprintf(folder, appPath);
				sprintf(file, "%s", PAL_FILE_NAME);
				break;
		}
		switch(method)
		{
			case METHOD_MC_SLOTA:
			case METHOD_MC_SLOTB:
				sprintf (temppath, "%s", file);
				temppath[31] = 0; // truncate filename
				break;
			default:
				sprintf (temppath, "/%s/%s", folder, file);
				break;
		}
	}
	CleanupPath(temppath); // cleanup path
	strncpy(filepath, temppath, MAXPATHLEN);
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

	return stricmp(((BROWSERENTRY *)f1)->filename, ((BROWSERENTRY *)f2)->filename);
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
			if(stricmp(p, ".7z") == 0)
				return true;
	}
	return false;
}

/****************************************************************************
 * StripExt
 *
 * Strips an extension from a filename
 ***************************************************************************/
void StripExt(char* returnstring, char * inputstring)
{
	char* loc_dot;

	strncpy (returnstring, inputstring, MAXJOLIET);

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
int BrowserLoadSz(int method)
{
	char filepath[MAXPATHLEN];
	memset(filepath, 0, MAXPATHLEN);

	// we'll store the 7z filepath for extraction later
	if(!MakeFilePath(szpath, FILE_ROM, method))
		return 0;

	// add device to filepath
	if(method != METHOD_DVD)
	{
		sprintf(filepath, "%s%s", rootdir, szpath);
		memcpy(szpath, filepath, MAXPATHLEN);
	}

	int szfiles = SzParse(szpath, method);
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
int BrowserLoadFile(int method)
{
	// store the filename (w/o ext) - used for sram/freeze naming
	StripExt(ROMFilename, browserList[browser.selIndex].filename);

	ROMLoaded = LoadVBAROM(method);
	inSz = false;

	if (!ROMLoaded)
	{
		ErrorPrompt("Error loading ROM!");
	}
	else
	{
		if (GCSettings.AutoLoad == 1)
			LoadBatteryOrStateAuto(GCSettings.SaveMethod, FILE_SRAM, SILENT);
		else if (GCSettings.AutoLoad == 2)
			LoadBatteryOrStateAuto(GCSettings.SaveMethod, FILE_SNAPSHOT, SILENT);
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
int BrowserChangeFolder(int method)
{
	if(inSz && browser.selIndex == 0) // inside a 7z, requesting to leave
	{
		if(method == METHOD_DVD)
			SetDVDdirectory(browserList[0].offset, browserList[0].length);

		inSz = false;
		SzClose();
	}

	if(!UpdateDirName(method))
		return -1;

	CleanupPath(browser.dir);
	strcpy(GCSettings.LoadFolder, browser.dir);

	switch (method)
	{
		case METHOD_DVD:
			ParseDVDdirectory();
			break;

		default:
			ParseDirectory(method);
			break;
	}

	if (!browser.numEntries)
	{
		ErrorPrompt("Error reading directory!");
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
	int method = GCSettings.LoadMethod;

	if(method == METHOD_AUTO)
		method = autoLoadMethod();

	// change current dir to roms directory
	switch(method)
	{
		case METHOD_DVD:
			browser.dir[0] = 0;
			if(MountDVD(NOTSILENT))
				if(ParseDVDdirectory()) // Parse root directory
					SwitchDVDFolder(GCSettings.LoadFolder); // switch to ROM folder
			break;
		default:
			sprintf(browser.dir, "/%s/", GCSettings.LoadFolder);
			CleanupPath(browser.dir);
			ParseDirectory(method); // Parse root directory
			break;
	}
	return browser.numEntries;
}

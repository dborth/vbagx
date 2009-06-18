/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * filebrowser.h
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/

#ifndef _FILEBROWSER_H_
#define _FILEBROWSER_H_

#include <unistd.h>
#include <gccore.h>

#define MAXJOLIET 255
#define MAXDISPLAY 37

typedef struct
{
	char dir[MAXPATHLEN]; // directory path of browserList
	int numEntries; // # of entries in browserList
	int selIndex; // currently selected index of browserList
	int pageIndex; // starting index of browserList page display
} BROWSERINFO;

typedef struct
{
	u64 offset; // DVD offset
	unsigned int length; // file length
	time_t mtime; // file modified time
	char isdir; // 0 - file, 1 - directory
	char filename[MAXJOLIET + 1]; // full filename
	char displayname[MAXDISPLAY + 1]; // name for browser display
} BROWSERENTRY;

extern BROWSERINFO browser;
extern BROWSERENTRY * browserList;
extern char rootdir[10];
extern char ROMFilename[512];
extern bool ROMLoaded;
extern char szpath[MAXPATHLEN];
extern bool inSz;

bool MakeFilePath(char filepath[], int type, int method, char * filename = NULL, int filenum = -2);
int UpdateDirName(int method);
int OpenGameList();
int autoLoadMethod();
int autoSaveMethod(bool silent);
int FileSortCallback(const void *f1, const void *f2);
void StripExt(char* returnstring, char * inputstring);
void ShortenFilename(char* returnstring, char * inputstring);
bool IsSz();
void ResetBrowser();
int BrowserLoadSz(int method);
int BrowserChangeFolder(int method);
int BrowserLoadFile(int method);

#endif

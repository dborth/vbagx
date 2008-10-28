/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * filesel.h
 *
 * Generic file routines - reading, writing, browsing
 ****************************************************************************/

#ifndef _NGCFILESEL_
#define _NGCFILESEL_

#include <unistd.h>

#define SAVEBUFFERSIZE (1024 * 1024 * 2)
#define MAXJOLIET 255
#define MAXDISPLAY 40

typedef struct
{
  u64 offset;
  unsigned int length;
  char flags;
  char filename[MAXJOLIET + 1];
  char displayname[MAXDISPLAY + 1];
} FILEENTRIES;

#define MAXFILES 2000 // Restrict to 2000 files per dir
extern FILEENTRIES filelist[MAXFILES];
extern unsigned char *savebuffer;
extern int offset;
extern int selection;
extern char currentdir[MAXPATHLEN];
extern char szpath[MAXPATHLEN];
extern bool inSz;
extern int maxfiles;
extern char ROMFilename[512];

void AllocSaveBuffer();
void FreeSaveBuffer();
bool MakeROMPath(char filepath[], int method);
int OpenROM (int method);
int autoLoadMethod();
int autoSaveMethod();
int FileSortCallback(const void *f1, const void *f2);
void StripExt(char* returnstring, char * inputstring);

#endif

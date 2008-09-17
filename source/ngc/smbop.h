/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * smbload.h
 *
 * SMB support routines
 ****************************************************************************/

#ifndef _NGCSMB_

#define _NGCSMB_

bool InitializeNetwork(bool silent);
bool ConnectShare (bool silent);
char * SMBPath(char * path);
int UpdateSMBdirname();
int ParseSMBdirectory ();
int LoadSMBFile (char *filename, int length);
int LoadBufferFromSMB (char *filepath, bool silent);
int LoadBufferFromSMB (char * sbuffer, char *filepath, bool silent);
int SaveBufferToSMB (char *filepath, int datasize, bool silent);

#endif

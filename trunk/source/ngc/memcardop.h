/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * memcardop.cpp
 *
 * Memory Card routines
 ***************************************************************************/

#ifndef _NGCMCSAVE_
#define _NGCMCSAVE_

int VerifyMCFile (char *buf, int slot, char *filename, int datasize);
int LoadMCFile (char *buf, int slot, char *filename, bool silent);
int SaveMCFile (char *buf, int slot, char *filename, int datasize, bool silent);
int MountCard(int cslot, bool silent);
bool TestCard(int slot, bool silent);

#endif

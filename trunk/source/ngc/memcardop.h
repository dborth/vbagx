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

int VerifyMCFile (unsigned char *buf, int slot, char *filename, int datasize);

int LoadBufferFromMC (unsigned char *buf, int slot, char *filename, bool silent);
int SaveBufferToMC (unsigned char *buf, int slot, char *filename, int datasize, bool silent);
int MountCard(int cslot, bool silent);
bool TestCard(int slot, bool silent);

#endif

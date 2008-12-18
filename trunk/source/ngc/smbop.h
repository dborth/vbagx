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

void InitializeNetwork(bool silent);
bool ConnectShare (bool silent);
void CloseShare();

#endif

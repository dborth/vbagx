/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * networkop.h
 *
 * Network and SMB support routines
 * Wii/GameCube only for now - SMB on Wii U is a later goal.
 ****************************************************************************/

#ifndef _NETWORKOP_H_
#define _NETWORKOP_H_

bool ConnectShare (bool silent);
void CloseShare();

#endif

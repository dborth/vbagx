/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vba.h
 *
 * This file controls overall program flow. Most things start and end here!
 ***************************************************************************/
#ifndef _VBA_H_
#define _VBA_H_

#include <gccore.h>
#define VERSIONNUM 		"1.0.4"
#define VERSIONSTR 		"VBA GX 1.0.4"
#define VERSIONSTRFULL 	"Visual Boy Advance GX 1.0.4"

#define NOTSILENT 0
#define SILENT 1

enum {
	METHOD_AUTO,
	METHOD_SD,
	METHOD_USB,
	METHOD_DVD,
	METHOD_SMB,
	METHOD_MC_SLOTA,
	METHOD_MC_SLOTB
};

struct SGCSettings{
    int		AutoLoad;
    int		AutoSave;
    int		LoadMethod; // For ROMS: Auto, SD, DVD, USB, Network (SMB)
	int		SaveMethod; // For SRAM, Freeze, Prefs: Auto, SD, Memory Card Slot A, Memory Card Slot B, USB, SMB
	char	LoadFolder[200]; // Path to game files
	char	SaveFolder[200]; // Path to save files
	char	CheatFolder[200]; // Path to cheat files
	char	gcip[16];
	char	gwip[16];
	char	mask[16];
	char	smbip[16];
	char	smbuser[20];
	char	smbpwd[20];
	char	smbgcid[20];
	char	smbsvid[20];
	char	smbshare[20];
    int		Zoom; // 0 - off, 1 - on
    float	ZoomLevel; // zoom amount
    int		widescreen;
	int		render;		// 0 - original, 1 - filtered, 2 - unfiltered
	int		VerifySaves;
};

extern struct SGCSettings GCSettings;
extern int ConfigRequested;

#endif

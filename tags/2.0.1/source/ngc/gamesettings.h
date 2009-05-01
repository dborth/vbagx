/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric October 2008
 *
 * gamesettings.h
 *
 * VBA support code
 ***************************************************************************/

#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

#include <gccore.h>

typedef struct gameSetting {
	char gameName[100];
	char gameID[5];
	int saveType;
	int rtcEnabled;
	int flashSize;
	int mirroringEnabled;
};

extern gameSetting gameSettings[];
extern int gameSettingsCount;

typedef struct gamePalette {
	char gameName[17];
	u32 palette[14]; // in 24-bit 0xRRGGBB
};

extern gamePalette gamePalettes[];
extern int gamePalettesCount;

#endif

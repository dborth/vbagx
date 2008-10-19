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

#endif

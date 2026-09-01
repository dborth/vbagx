/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * gamesettings.h
 *
 * VBA support code
 ***************************************************************************/

#ifndef GAMESETTINGS_H
#define GAMESETTINGS_H

#include <gccore.h>

struct gamePalette {
	char gameName[17];
	char use;
	uint32_t palette[14]; // in 24-bit 0xRRGGBB
};

extern gamePalette gamePalettes[];
extern int gamePalettesCount;
extern gamePalette CurrentPalette;

#endif

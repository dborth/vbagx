/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vbasupport.h
 *
 * VBA support code
 ***************************************************************************/

#ifndef VBASUPPORT_H
#define VBASUPPORT_H

#include "System.h"

extern struct EmulatedSystem emulator;
extern int cartridgeType;
extern int SunBars;
extern u32 RomIdCode;
extern bool TiltSideways;
extern char RomTitle[];

bool LoadVBAROM();
void InitialisePalette();
bool IsGameboyGame();
bool LoadBatteryOrState(char * filepath, int action, bool silent);
bool LoadBatteryOrStateAuto(int action, bool silent);
bool SaveBatteryOrState(char * filepath, int action, bool silent);
bool SaveBatteryOrStateAuto(int action, bool silent);

#endif

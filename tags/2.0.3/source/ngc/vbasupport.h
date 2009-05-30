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

bool LoadVBAROM(int method);
void InitialisePalette();
bool LoadBatteryOrState(char * filepath, int method, int action, bool silent);
bool LoadBatteryOrStateAuto(int method, int action, bool silent);
bool SaveBatteryOrState(char * filepath, int method, int action, bool silent);
bool SaveBatteryOrStateAuto(int method, int action, bool silent);

#endif

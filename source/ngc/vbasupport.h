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
extern u32 loadtimeradjust;
extern int cartridgeType;
extern int SunBars;
extern u32 RomIdCode;
extern bool TiltSideways;

bool LoadVBAROM(int method);
void InitialisePalette();
bool LoadBatteryOrState(int method, int action, bool silent);
bool SaveBatteryOrState(int method, int action, bool silent);

#endif

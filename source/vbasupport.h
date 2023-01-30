/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric 2008-2023
 *
 * vbasupport.h
 *
 * VBA support code
 ***************************************************************************/

#ifndef VBASUPPORT_H
#define VBASUPPORT_H

#include "vba/System.h"

extern struct EmulatedSystem emulator;
extern int cartridgeType;
extern int SunBars;
extern u32 RomIdCode;
extern bool TiltSideways;
extern char RomTitle[];

bool LoadVBAROM();
void InitialisePalette();
bool IsGameboyGame();
bool IsGBAGame();
void ResetTiltAndCursor();
bool LoadBatteryOrState(char * filepath, int action, bool silent);
bool LoadBatteryOrStateAuto(int action, bool silent);
bool SaveBatteryOrState(char * filepath, int action, bool silent);
bool SaveBatteryOrStateAuto(int action, bool silent);
bool SavePreviewImg (char * filepath, bool silent);

#endif

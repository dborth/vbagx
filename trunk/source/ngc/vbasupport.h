/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * vbasupport.h
 *
 * VBA support code
 ***************************************************************************/

#include "System.h"

extern struct EmulatedSystem emulator;
extern u32 loadtimeradjust;
bool LoadVBAROM(int method);
void InitialisePalette();
bool LoadBatteryOrState(int method, int action, bool silent);
bool SaveBatteryOrState(int method, int action, bool silent);

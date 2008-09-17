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
int loadVBAROM(char filename[]);
void InitialisePalette();
bool LoadBattery(int method, bool silent);
bool SaveBattery(int method, bool silent);
bool LoadState(int method, bool silent);
bool SaveState(int method, bool silent);

/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * vbasupport.h
 *
 * VBA support code
 ***************************************************************************/

#ifndef VBASUPPORT_H
#define VBASUPPORT_H

#include "vba/System.h"

enum {
	CARTRIDGE_NONE = 0,
	CARTRIDGE_GB,
	CARTRIDGE_GBA
};

extern struct EmulatedSystem emulator;
extern int cartridgeType;
extern int SunBars;
extern uint32_t RomIdCode;
extern bool TiltSideways;
extern char RomTitle[];
extern int GBAROMSize;

float systemGetRenderFPS();
float systemGetCoreFPS();
void systemResetPacer();
bool LoadVBAROM();
void RomCleanup();
void InitGBGame();
void InitGameDimensionsAndBorder();
void InitialisePalette();
bool IsGameboyGame();
bool IsGBAGame();
void ResetTiltAndCursor();
bool LoadBatteryOrState(char * filepath, int action, bool silent);
bool LoadBatteryOrStateAuto(int action, bool silent);
bool SaveBatteryOrState(char * filepath, int action, bool silent);
bool SaveBatteryOrStateAuto(int action, bool silent);

// Deferred auto-save: SnapshotBatteryOrStateAuto() copies the battery/state and its
// destination right now (call it from the thread that owns the emulator, while the
// game is still loaded); WriteBatteryOrStateSnapshot() does the device I/O later,
// on any thread. SnapshotBatteryOrStateAuto() returns nullptr if there is nothing to
// save. The snapshot must be freed with FreeBatteryOrStateSnapshot(), which accepts
// nullptr.
struct BatteryOrStateSnapshot;
BatteryOrStateSnapshot * SnapshotBatteryOrStateAuto(int action);
bool WriteBatteryOrStateSnapshot(BatteryOrStateSnapshot * snapshot, bool silent);
void FreeBatteryOrStateSnapshot(BatteryOrStateSnapshot * snapshot);
bool SavePreviewImg (char * filepath, bool silent);

#endif

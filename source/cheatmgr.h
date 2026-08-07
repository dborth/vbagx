/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2026
 *
 * cheatmgr.h
 *
 * Cheat manager
 ****************************************************************************/

#ifndef _CHEATMGR_H_
#define _CHEATMGR_H_

#define MAX_CHEATS      150

struct Cheat {
    const char* name;     // Pointer into contiguous string pool
    const char* rawCode;  // Pointer into contiguous string pool
    bool enabled;
    int vbaStartIndex;    // Index of first sub-code in VBA-M's array when active
    int vbaCodeCount;     // How many sub-codes were added to VBA-M
};

extern Cheat cheats[];
extern int cheatCount;

void ResetCheats();
void LoadCheatFile();
void ToggleCheat(int num);

#endif

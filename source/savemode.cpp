/****************************************************************************
 * VBA GX
 *
 * Daryl Borth 2008-2026
 *
 * savemode.cpp
 *
 * Save Mode: what is saved and loaded without being asked (see savemode.h)
 ***************************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "vbagx.h"
#include "savemode.h"
#include "vbasupport.h"
#include "fileop.h"
#include "filebrowser.h"
#include "menu/menu.h"

#include "vba/System.h"


// What is in storage is behind what is in the machine, for each of the two autosaves. They are kept apart
// because they are written apart: the Save Mode can change in the menu, and either file can be updated on
// request, which brings only that one up to date.
static bool snapshotStale = false;
static bool cartridgeStale = false;

// The screenshot taken when the game was left no longer shows the machine
static bool screenshotStale = false;

void MarkLiveStateChanged()
{
	snapshotStale = true;
	cartridgeStale = true;
	screenshotStale = true;
}

void ClearLiveStateChanged()
{
	snapshotStale = false;
	cartridgeStale = false;
}

bool LiveStateChanged()
{
	switch(EmuSettings.saveMode)
	{
		case SAVEMODE_SNAPSHOT:
			return snapshotStale;
		case SAVEMODE_GAMESAVE:
			return cartridgeStale;
		default:
			return false;
	}
}

void AutoSnapshotWritten()
{
	snapshotStale = false;
}

void CartridgeSaveWritten()
{
	cartridgeStale = false;
}

void MarkScreenshotCurrent()
{
	screenshotStale = false;
}

bool ScreenshotIsCurrent()
{
	return !screenshotStale;
}

/****************************************************************************
 * LoadAutoSnapshot
 *
 * \return 1 if the Auto Snapshot was loaded, 0 if there is none, -1 if there is
 * one that couldn't be loaded
 ***************************************************************************/
static int LoadAutoSnapshot()
{
	char filepath[MAXPATHLEN];

	if(!MakeFilePath(filepath, FILE_STATE, ROMFilename, 0))
		return 0;

	FILE * fp = fopen(filepath, "rb");

	if(!fp)
		return 0;

	fclose(fp);
	return LoadBatteryOrState(filepath, FILE_STATE, SILENT) ? 1 : -1;
}

/****************************************************************************
 * AutoLoadOnGameOpen
 *
 * The game has just been loaded and is cold booted. Runs on the thread that
 * loaded it.
 ***************************************************************************/
void AutoLoadOnGameOpen()
{
	MigrateSaveFileNames();

	switch(EmuSettings.saveMode)
	{
		case SAVEMODE_SNAPSHOT:
		{
			const int snapshot = LoadAutoSnapshot();

			if(snapshot == 1)
				break;

			// A Snapshot that failed part way leaves nothing to carry on from
			if(snapshot < 0)
				emulator.emuReset();

			// No Auto Snapshot yet (the first time, or the Save Mode was just
			// changed), or one that can't be loaded: the Cartridge Save is what is left
			const bool gameSave = LoadBatteryOrStateAuto(FILE_SRAM, SILENT);

			if(snapshot < 0)
			{
				if(gameSave)
					ErrorPrompt("The Auto Snapshot couldn't be loaded. The Cartridge Save was loaded instead.");
				else
					ErrorPrompt("The Auto Snapshot couldn't be loaded. The game starts from the beginning.");
			}
			break;
		}

		case SAVEMODE_GAMESAVE:
			LoadBatteryOrStateAuto(FILE_SRAM, SILENT);
			break;

		case SAVEMODE_MANUAL:
		default:
			break;
	}

	ClearLiveStateChanged(); // what was just loaded is what is in storage
}

/****************************************************************************
 * AutoSaveOnQuit
 *
 * Leaving the game for good. In the game, what it is doing now is written. In
 * the menu, nothing was written when the game was left for it, so what the
 * game was doing then - or what was loaded or reset since - is written now,
 * if the autosave is behind it.
 ***************************************************************************/
void AutoSaveOnQuit(bool screenshotIsCurrent)
{
	if(!ROMLoaded)
		return;

	const bool inMenu = appRequest == AppRequest::MENU;

	switch(EmuSettings.saveMode)
	{
		case SAVEMODE_SNAPSHOT:
		{
			if(inMenu && !snapshotStale)
				break;

			// A write to the same file may still be going
			WaitForBackgroundTasks(15000);

			// In the menu, the screenshot is right unless something was loaded or reset there
			BatteryOrStateSnapshot * snapshot = SnapshotBatteryOrStateAuto(FILE_STATE, inMenu ? ScreenshotIsCurrent() : screenshotIsCurrent);

			if(snapshot)
			{
				WriteBatteryOrStateSnapshot(snapshot, SILENT);
				FreeBatteryOrStateSnapshot(snapshot);
			}
			AutoSnapshotWritten();
			break;
		}

		case SAVEMODE_GAMESAVE:
			if(inMenu && !cartridgeStale)
				break;

			WaitForBackgroundTasks(15000);
			SaveBatteryOrStateAuto(FILE_SRAM, SILENT);
			CartridgeSaveWritten();
			break;

		case SAVEMODE_MANUAL:
		default:
			break;
	}
}

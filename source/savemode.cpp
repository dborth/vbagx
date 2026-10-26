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


static bool liveStateChanged = false;

void MarkLiveStateChanged()
{
	liveStateChanged = true;
}

void ClearLiveStateChanged()
{
	liveStateChanged = false;
}

bool LiveStateChanged()
{
	return liveStateChanged;
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
			// changed), or one that can't be loaded: the Game Save File is what is left
			const bool gameSave = LoadBatteryOrStateAuto(FILE_SRAM, SILENT);

			if(snapshot < 0)
			{
				if(gameSave)
					ErrorPrompt("The Auto Snapshot couldn't be loaded. The Game Save File was loaded instead.");
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
 ***************************************************************************/
void AutoSaveOnQuit(bool screenshotIsCurrent)
{
	if(!ROMLoaded)
		return;

	// In the menu, the game was already saved when it was left
	const bool inMenu = appRequest == AppRequest::MENU;

	switch(EmuSettings.saveMode)
	{
		case SAVEMODE_SNAPSHOT:
		{
			// Something loaded or reset in the menu since then is what the game
			// should come back to
			if(inMenu && !LiveStateChanged())
				break;

			// The write from leaving the game may still be going to the same file
			WaitForBackgroundTasks(15000);

			BatteryOrStateSnapshot * snapshot = SnapshotBatteryOrStateAuto(FILE_STATE, !inMenu && screenshotIsCurrent);

			if(snapshot)
			{
				WriteBatteryOrStateSnapshot(snapshot, SILENT);
				FreeBatteryOrStateSnapshot(snapshot);
			}
			ClearLiveStateChanged();
			break;
		}

		case SAVEMODE_GAMESAVE:
			if(!inMenu)
				SaveBatteryOrStateAuto(FILE_SRAM, SILENT);
			break;

		case SAVEMODE_MANUAL:
		default:
			break;
	}
}

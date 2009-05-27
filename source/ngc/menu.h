/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * menu.h
 *
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#ifndef _NGCMENU_
#define _NGCMENU_

#include <ogcsys.h>
#include "gui/gui.h"

extern GuiImageData * pointer[4];

void InitGUIThreads();
void MainMenu (int menuitem);
void ErrorPrompt(const char * msg);
int ErrorPromptRetry(const char * msg);
void InfoPrompt(const char * msg);
void ShowAction (const char *msg);
void CancelAction();
void ShowProgress (const char *msg, int done, int total);

enum
{
	MENU_EXIT = -1,
	MENU_NONE,
	MENU_SETTINGS,
	MENU_SETTINGS_FILE,
	MENU_SETTINGS_MENU,
	MENU_SETTINGS_NETWORK,
	MENU_GAMESELECTION,
	MENU_GAME,
	MENU_GAME_SAVE,
	MENU_GAME_LOAD,
	MENU_GAMESETTINGS,
	MENU_GAMESETTINGS_MAPPINGS,
	MENU_GAMESETTINGS_MAPPINGS_MAP,
	MENU_GAMESETTINGS_VIDEO,
	MENU_GAMESETTINGS_CHEATS,
	MENU_GAMESETTINGS_PALETTE
};

#endif

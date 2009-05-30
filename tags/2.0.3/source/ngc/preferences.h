/****************************************************************************
 * Visual Boy Advance GX
 *
 * Tantric September 2008
 *
 * preferences.h
 *
 * Preferences save/load to XML file
 ***************************************************************************/

bool SavePrefs (bool silent);
bool LoadPrefs ();
bool SavePalette (bool silent, const char *gameName);
bool LoadPalette (const char *gameName);

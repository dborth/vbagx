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
bool SavePalettes (bool silent);
bool LoadPalettes();
void SetPalette(const char *gameName);

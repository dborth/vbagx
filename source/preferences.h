/****************************************************************************
 * Visual Boy Advance GX
 *
 * Daryl Borth 2008-2026
 *
 * preferences.h
 *
 * Preferences save/load to XML file
 ***************************************************************************/

bool SavePrefs();
bool LoadPrefs();
void CreateMissingDirectories();
void FixInvalidSettings();
void DefaultSettings();
void ApplySettings();
bool SavePalettes(bool silent);
bool LoadPalettes();
void SetPalette(const char *gameName);
bool SavePaletteAs(bool silent, const char *name);

# Visual Boy Advance GX
https://github.com/dborth/vbagx (Under GPL License)
 
Visual Boy Advance GX is a modified port of VBA-M.
With it you can play GBA/Game Boy Color/Game Boy games on your Wii/GameCube.


## TABLE OF CONTENTS
 - [Nightly Builds](#nightly-builds)
 - [Features](#features)
 - [Update History](#update-history)
 - [Setup & Installation](#setup--installation)
 - [Instructions](#instructions)
 - [Credits](#credits)
 - [Links](#links)


## NIGHTLY BUILDS

|Download nightly builds from continuous integration: 	| [![Build Status][Build]][Actions]
|-------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------|

[Actions]: https://github.com/dborth/vbagx/actions/workflows/continuous-integration-workflow.yml
[Build]: https://github.com/dborth/vbagx/actions/workflows/continuous-integration-workflow.yml/badge.svg


## FEATURES

* Wiimote, Nunchuk, Classic, Wii U Pro, and Gamecube controller support
* Wii U GamePad support (requires homebrew injection into Wii U VC title)
* Rotation sensors, Solar sensors, and Rumble support
* Optional special Wii controls built-in for some games
* SRAM and State saving
* IPS/UPS patch support
* Custom controller configurations
* SD, USB, DVD, SMB, Zip, and 7z support
* Compatibility based on VBA-M r1231
* MEM2 ROM Storage for fast access
* Auto frame skip (optional) for those core heavy games
* Turbo speed, video zooming, widescreen, and unfiltered video options
* Native loading/saving of ROMS and SRAM from Goomba (a GB emulator for GBA)
* Improved video rendering from RetroArch
* Screenshots can be displayed on the main menu
* Fixed pixel ratio mode (1x, 2x, and 3x)
* Borders (from Super Game Boy games or custom from .png)
* 240p support


## UPDATE HISTORY

[2.4.5 - March 23, 2021]

* Added L+R+START for back to menu for Wii Classic Controller
* Updated French translation (thanks Tanooki16!)
* Fixed issue with displaying screenshots

[2.4.4 - February 6, 2021]

* Fixed SD2SP2 / SD gecko issues (again)

[2.4.3 - January 31, 2021]

* Fixed SD2SP2 issues
* Changed max game image dimensions to 640x480 to support screenshots

[2.4.2 - January 18, 2021]

* Compiled with latest devkitPPC/libogc
* Added ability to change the player mapped to a connected controller
* Significant memory usage reductions (fonts and loading cover images)
* Other minor fixes

[2.4.1 - June 29, 2020]

* Compiled with latest devkitPPC/libogc
* Fixed some 3rd party controllers with invalid calibration data
* Translation updates
* Added Wii U vWii Channel, widescreen patch, and now reports console/CPU speed
* Added support for serial port 2 (SP2 / SD2SP2) on Gamecube
* Fixed Wii U Pro controller button mapping not being used in one case
* Fixed ZL button mapping for Wii U GamePad
* Other minor fixes

[2.4.0 - April 13, 2019]

* Fixed crash when used as wiiflow plugin
* Fixed crash on launch when using network shares
* Fixed issues with on-screen keyboard
* Updated Korean translation

[2.3.9 - January 25, 2019]

* Added ability to load external fonts and activated Japanese/Korean 
  translations. Simply put the ko.ttf or jp.ttf in the app directory
* Added ability to customize background music. Simply put a bg_music.ogg
  in the app directory
* Added ability to change preview image source with + button (thanks Zalo!)
* Fixed issue with resetting motion controls
* Fixed issue with Mode 0 graphics transparency

[2.3.8 - January 4, 2019]

* Restored changes lost from 2.3.0 core upgrade (GameCube virtual memory, 
  optimizations from dancinninjac, GB color palettes, rotation/tilt for 
  WarioWare Twisted, in-game rumble) 
* Improved WiiFlow integration
* Fixed controllers with no analog sticks
* Added Wii U GamePad support (thanks Fix94!)

[2.3.7 - August 28, 2018]

* Allow loader to pass two arguments instead of three (libertyernie)
* don't reset settings when going back to an older version
* Fix a few potential crashes caused by the GUI
* Other minor fixes/improvements
* Compiled with latest libOGC/devkitPPC

[2.3.6 - December 11, 2016]

* Restored Wiiflow mode plugin by fix94
* Restored fix filebrowser window overlapping
* Change all files End Of Line to windows mode
* Remove update check for updates

[2.3.5 - December 10, 2016]

* Hide saving dialog that pops up briefly when returning from a game

[2.3.4 - September 15, 2016]

* Added the delete save file (SRAM / Snapshot) option
* Changed the box colors for the SRAM and Snapshots files to match the color 
  scheme of the emu GUI
* Change the "Power off Wii" exit option to completely turn off the wii, 
  ignoring the WC24 settings
* Updated settings file name in order to have it's own settings file name
* Added an option to switch between screenshots, covers, or artwork images, 
  with their respective named folders at the device's root. You can set which
  one to show, by going to Settings > Menu > Preview Image. The .PNG image file
  needs to have the same name as the ROM (e.g.: Mother 3.png)
* Removed sound from GUI (thanks to Askot) 
* Added option to switch between the Green or Monochrome GB color screen. You 
  can set which one to show by going to Settings > Emulation > GB Screen Palette

[2.3.3 - June 25, 2016]

* Fixed the GC pad Down input on the File browser window
* Added Koston's green gb color screen
* Added the Screenshot Button
* Increased and Centered the Screenshot image and reduce game list width
* Added a background for the preview image
* Added the WiiuPro Controller icon on the controller settings
* Fix DSI error / Bug from Emulator Main Menu

[2.3.2 - March 4, 2015] - libertyernie

* Wii U: if widescreen is enabled in the Wii U setting, VBA GX will use a 16:9
  aspect ratio, except while playing a game with fixed pixel mode turned on
* There are now three options for border in the emulation settings menu (see
  "Super Game Boy borders" section for details)
  * PNG borders now supported for GBA games
* Video mode "PAL (50Hz)" renamed to "PAL (576i)"
* Video mode "PAL (60Hz)" renamed to "European RGB (480i)"
* 240p support added (NTSC and European RGB modes)
* All video modes now use a width of 704 for the best pixel aspect ratio

[2.3.1b - November 8, 2014] - Glitch

* Added FIX94's libwupc for WiiU Pro Controllers
* Added tueidj's vWii Widescreen Fix

[2.3.1 - October 14, 2014] - libertyernie

* Super Game Boy border support
  * Borders can be loaded from (and are automatically saved to) PNG files
  * Any border loaded from the game itself will override the custom PNG border
* Custom palette support from 2.2.8 restored
* Option added to select Game Boy hardware (GB/SGB/GBC/auto)
* Fixed pixel ratio mode added
  * Overrides zoom and aspect ratio settings
  * To squish the picture so it appears correctly on a 16:9 TV, you can open
    the settings.xml file and add 10 to the gbFixed/gbaFixed value. However,
	setting your TV to 4:3 mode will yield a better picture.
* Real-time clock fixes for GB/GBC games, including Pokémon G/S/C
  * RTC data in save file stored as little-endian
  * Option added for UTC offset in the main menu (only required if you use the
    same SRAM on other, time-zone-aware platforms)
* New option for selecting "sharp" or "soft" filtering settings
  * "Sharp" was the default for 480p, "soft" was the default for 480i

[2.3.0 - September 10, 2014] - libertyernie

* VBA-M core updated to r1231
* Tiled rendering used for GBA games (new VBA-M feature, originally from
  RetroArch) - provides a major speed boost!
* Changes from cebolleto's version
  * Screenshots can be displayed for each game on the menu
  * Nicer 7-Zip support
  * When you leave a folder, the folder you just left will be selected
* New options available:
  * Disable the " Auto" string being appended to save files
  * Disable frameskip entirely on GBA
* Keyboard fixed (from libwiigui r56)
* GUI prompt is now purple instead of green (button colors more intuitive)
* Goomba and Goomba Color ROM support:
  * Any Game Boy ROM stored within a Goomba ROM can be loaded "natively" in
    the Game Boy (Color) emulator (or the Goomba ROM can be loaded as GBA)
  * Game Boy SRAM stored within Goomba SRAM is loaded and saved correctly

[2.2.8 - July 29, 2012]

* Fixed lag with GameCube controllers

[2.2.7 - July 7, 2012]

* Fixed PAL support

[2.2.6 - July 6, 2012]

* Support for newer Wiimotes
* Fixed missing audio channel bug (eg: in Mario & Luigi: Superstar Saga)
* Improved controller behavior - allow two directions to be pressed simultaneously
* Compiled with devkitPPC r26 and libogc 1.8.11

[2.2.5 - May 15, 2011]

* Added Turkish translation

[2.2.4 - March 23, 2011]

* Fixed browser regressions with stability and speed

[2.2.3 - March 19, 2011]

* Improved USB and controller compatibility (recompiled with latest libogc)
* Enabled SMB on GameCube (thanks Extrems!)
* Added Catalan translation
* Translation updates

[2.2.2 - October 7, 2010]

* Fixed "blank listing" issue for SMB
* Improved USB compatibility and speed
* Added Portuguese and Brazilian Portuguese translations
* Channel updated (improved USB compatibility)
* Other minor changes

[2.2.1 - August 14, 2010]

* IOS 202 support removed
* USB 2.0 support via IOS 58 added - requires that IOS58 be pre-installed
* DVD support via AHBPROT - requires latest HBC

[2.2.0 - July 22, 2010]

* Fixed broken auto-update

[2.1.9 - July 20, 2010]

* Reverted USB2 changes

[2.1.8 - July 14, 2010]

* Ability to use both USB ports (requires updated IOS 202 - WARNING: older 
  versions of IOS 202 are NO LONGER supported)
* Hide non-ROM files
* Other minor improvements

[2.1.7 - June 20, 2010]

* USB improvements
* GameCube improvements - audio, SD Gecko, show thumbnails for saves
* Other minor changes

[2.1.6 - May 19, 2010]

* DVD support fixed
* Fixed some potential hangs when returning to menu
* Video/audio code changes
* Fixed scrolling text bug
* Other minor changes

[2.1.5 - April 9, 2010]

* Fix auto-save bug

[2.1.4 - April 9, 2010]

* Fixed issue with saves (GBA) and snapshots (GB)
* Most 3rd party controllers should work now (you're welcome!)
* Translation updates (German and Dutch)
* Other minor changes

[2.1.3 - March 30, 2010]

* Fixed ROM allocation. Should solve some unexplained crashes
* Numerous performance optimizations (thanks dancinninja!)
* DVD / USB 2.0 support via IOS 202. DVDx support has been dropped. It is
  highly recommended to install IOS 202 via the included installer
* Multi-language support (only French translation is fully complete)
* Thank you to everyone who submitted translations
* SMB improvements/bug fixes
* Minor video & input performance optimizations
* Disabling rumble now also disables in-game rumbling
* Fixed saving of GB screen position adjustment

[2.1.2 - December 23, 2009]

* Numerous core optimizations (thanks dancinninjac!)
* File browser now scrolls down to the last game when returning to browser
* Auto update for those using USB now works
* Fixed scrollbar up/down buttons
* Minor optimizations

[2.1.1 - December 7, 2009]

* Save state corruption issues fixed

[2.1.0 - December 2, 2009]

* Fixed SMB (for real this time!)

[2.0.9 - November 30, 2009]

* Fixed SMB
* Added separate horizontal/vertical zoom options, and separate GB/GBA ones
* Improved scrolling timing - the more you scroll, the fast it goes
* Fixed reset button on Wii console - now you can reset multiple times
* APU optimization (dancinninjac)
* Minor code optimizations
* Reduce memory fragmentation - fixes out of memory crashes

[2.0.8 - October 7, 2009]

* Revamped filebrowser and file I/O
* Fixed MBC2 saving/loading
* Fixed some GB-Z80 instructions
* DVD loading in GameCube should work now (untested and unsupported)
* Many, many other bug fixes

[2.0.7 - September 16, 2009]

* Text rendering corrections
* SMB improvements
* Built with latest libraries
* Video mode switching now works properly
* Other minor bugfixes and cleanup

[2.0.6 - July 22, 2009]

* Fixed "No game saves found." message when there are actually saves.
* Fixed shift key on keyboard
* Text scrolling works again
* Change default prompt window selection to "Cancel" button

[2.0.5 - July 9, 2009]

* Faster SMB/USB browsing
* Last browsed folder is now remembered
* Fixed controller mapping reset button
* Fixed no sound on GameCube version
* Directory names are no longer altered
* Preferences now only saved on exit
* Fixed on-screen keyboard glitches
* SRAM auto-saved on power-off from within a game
* Prevent 7z lockups, better 7z error messages

[2.0.4 - June 30, 2009]

* Fixed auto-update
* Increased file browser listing to 10 entries, decreased font size
* Added text scrolling on file browser
* Added reset button for controller mappings
* Settings are now loaded from USB when loading the app from USB on HBC
* Fixed menu crashes caused by ogg player bugs
* Fixed memory card saving verification bug
* Fixed game savebrowser bugs
* Miscellaneous code cleanup/corrections

[2.0.3 - May 30, 2009]

* Fixed SD/USB corruption bug
* SMB works again
* GUI bugs fixed, GUI behavioral improvements
* GB Palette editing
* More built-in palettes
* Palettes now fade to white correctly instead of getting brighter
* Can now turn off palette colorizing
* Workaround for palette issue on Mega Man I GB - palette disabled
* Star Wars, TMNT, Lord Of The Rings, Castlevania Wii Controls
* Fix for WarioWare startup - Nunchuk C button or Wii Remote B button will now
  make calibration easy by locking the gyroscope.
* Fixed issues with constant rumbling

[2.0.2 - May 26, 2009]

* Improved stability
* Fixed broken SDHC from HBC 1.0.2 update
* Fixed issues with returning to menu from in-game
* Add option to disable rumble
* Auto-determines if HBC is present - returns to Wii menu otherwise
* Unfiltered mode fixed
* Miscellaneous bugfixes

[2.0.1 - April 30, 2009]

* Multiple state saves now working
* Built with more stable libogc/libfat
* Fixed settings saving glitches
* Fixed Mortal Kombat GameCube controller bug
* Fixed Zelda DX palette bug
* Fixed Harry Potter 1-3 keyboard bug

[2.0.0 - April 27, 2009]

* New GX-based menu, with a completely redesigned layout. Has Wiimote IR 
  support, sounds, graphics, animation effects, and more
* Thanks to the3seashells for designing some top-notch artwork, to 
  Peter de Man for composing the music, and a special thanks to shagkur for 
  fixing libogc bugs that would have otherwise prevented the release
* Onscreen keyboard for changing save/load folders and network settings
* Menu configuration options (configurable exit button, wiimote orientation,
  volumes)
* New save manager, allowing multiple saves and save browsing. Shows
  screenshots for Snapshot saves, and save dates/times
* Added video shifting option
* Added video mode selection (recommended to leave on Automatic)
* USB Mouse support (buttons only)
* Keyboard shift key bug fixed
* Built-in 14 colour palettes for some monochrome gameboy games (Magnetic
  Soccer, Malibu Beach Volleyball, Marble Madness, Metroid 2, Mortal Kombat,
  Mortal Kombat II, Mortal Kombat 3, Mr. Do!)  
* Rumble works in GBC games designed for rumble cartridges but shipped
  without rumble cartridges, such as Disney's Tarzan for GBC
* Improved Mortal Kombat Wii Controls
* Mortal Kombat games now have many extra characters to choose
* Wii Controls for more Teenage Mutant Ninja Turtles games
* Improved Lego Star Wars controls
* Boktai menu now tells you when there can't be sun because it is night
* Minor bug fixes

[1.0.9 - April 7, 2009]

* Gamecube controller should no longer rumble constantly

[1.0.8 - April 4, 2009]

* "Match Wii Game" controls option! Games that have a Wii equivalent can be
  played using the controls for that Wii game. For example all Zelda games
  can be played with Twilight Princess controls. See the Instructions section
  below for important details.
* Rotation/Tilt sensor games all work
* Solar sensors (Boktai 1/2/3)
* Rumble (except for games that rely on Gameboy Player)
* Keyboard
* PAL support, finally!
* New scaling options, choose how much stretching you want
* Colourised games now partially work but still have distortion
* "Corvette" no longer has a screwed up palette (but still crashes)
* Triggers net reconnection on SMB failure
* Source code refactored, and project file added
* Instructions section added to this readme file

[1.0.7 - January 27, 2009]

* Updated to VBA-M r847
* Corrected sound interpolation
* Faster SD/USB - new read-ahead cache
* Removed trigger of back to menu for Classic Controller right joystick
* Fixed a bug with reading files < 2048 bytes
* Fixed GBA games on GameCube
* Fixed homebrew GBA games on GameCube
* Fixed some memory leaks, buffer overflows, etc
* Code cleanup, other general bugfixes

[1.0.6 - December 24, 2008]

* Fixed save state saving bug
* Fixed unstable SD card access
* Proper SD/USB hotswap (Wii only)
* Auto-update feature (Wii only)
* Rewritten SMB access - speed boost, NTLM now supported (Wii only)
* Improved file access code
* Resetting preferences now resets controls
* Minor bug fixes

[1.0.5 - November 19, 2008]

* SDHC works now
* Frameskipping tweaks
* Fixed snapshot loading issue
* Full widescreen support
* Changed scaling
* Zooming fixed (thanks eke-eke!)
* PAL timing changes - EURGB60 mode forced
* Wii - Added console/remote power button support
* Wii - Added reset button support (resets game)
* Wii - Settings file is now named settings.xml and is stored in the same
  folder as the DOL (eg: apps/vbagx/settings.xml)
* GameCube - Added DVD motor off option
* GameCube - Fixed GBA loading issue

[1.0.4 - October 28, 2008]

* Complete port of VBA-M - now uses blaarg's new audio core, latest GB core
* Frameskipping improvements
* Sound processing improved - L-R channel reversal corrected, skipping fixed
* Saving problems fixed, game compatibility improved
* IPS/UPS/PPF patch support
* SD/USB hot-swapping!
* SDHC support
* Zoom setting saved
* Widescreen correction option
* GameCube support is back, including Qoob support!

[1.0.3 - October 15, 2008]

* New timing / frameskip algorithm - should (hopefully) work 100% better!
* Performance improvements - video threading, PPC core partly activated
* Video zooming option
* Unfiltered video option
* 7z support
* Loading progress bars added

[1.0.2 - October 6, 2008]

* New core! The core is now a custom combination of VBA-M and VBA 1.72
* Added DVD, SMB, ZIP, GameCube MC support
* Faster USB/SD speeds
* Screen alignment and flickering problems fixed
* 128K save support added
* Better emulation speeds. Should now be nearly full speed all the time
  for most games.
* Turbo speed feature. Mapped to right C-stick (classic controller & 
  Gamecube controller), and A+B for wiimote
* Controller mapping preferences bug fixed. Your preferences will reset
  automatically to correct any problems in your preferences file
* Many other tweaks behind the scenes

[1.0.1 - September 18, 2008]

* GBA games now run at full speed
* Menu improvements, with spiffy new background
* Fixed L/R buttons - they work now

[1.0.0 - September 16, 2008]

* Now compiles with devkitpro r15
* One makefile to make all versions
* Complete rewrite based on code from SNES9x GX
* Now has a menu! ROM selector, preferences, controller mapping, etc
* Wiimote, Nunchuk, and Classic controller support
* Button mapping for all controller types
* Full support for SD and USB
* Load/save preference selector. ROMs, saves, and preferences are 
  saved/loaded according to these
* 'Auto' settings for save/load - attempts to automatically determine
  your load/save device(s) - SD, USB
* Preferences are loaded and saved in XML format. You can open
  VBAGX.xml edit all settings, including some not available within
  the program


## SETUP & INSTALLATION

Unzip the archive. You will find the following folders inside:

apps			Contains Homebrew Channel ready files
				(see Homebrew Channel instructions below)

vbagx			Contains the directory structure required for storing
				roms and saves. By default, roms are loaded from 
				"vbagx/roms/" and saves / preferences are stored in 
				"vbagx/saves/".
		
		
### Loading / Running the Emulator:

#### Wii - Via Homebrew Channel:
The most popular method of running homebrew on the Wii is through the Homebrew
Channel. If you already have the channel installed, just copy over the apps folder
included in the archive into the root of your SD card.

Remember to also create the vbagx directory structure required. See above.

If you haven't installed the Homebrew Channel yet, read about how to here:
http://hbc.hackmii.com/

#### Gamecube:
You can load VBAGX via sdload and an SD card in slot A, or by streaming 
it to your Gamecube, or by booting a bootable DVD with VBAGX on it. 
This document doesn't cover how to do any of that.
 

## INSTRUCTIONS

If you have upgraded from a previous version, the emulator may start with a
message that your preferences have been reset. You will need to set your 
preferences how you want them.

Otherwise the emulator will start at the main menu, which is a list of game
ROMs. There is also a settings button to choose how and where to load or
save files, and to change menu settings.

Navigate the menu with the D-Pad, or the Wiimote pointer, and select options 
with the A button. Press the B button to swap between controlling a list box 
and controlling the buttons. Pressing the Home button will exit from the
main menu. You can choose what exiting will do by using the settings menu.

Click on the logo to see the credits.

When choosing a file, use left and right to go up or down a page.

Once you choose a game, the game will start. But you can get back to a menu
by pressing Home. This takes you to the in-game menu, where you can save,
load, reset, or change settings. The settings apply to all games, not just
the current one. These settings are different from the settings on the main
menu. If you are playing a Boktai game with a solar sensor, there will also
be a fifth button which lets you change the weather. The sunlight is based
on the weather, the time of day, and the angle of your Wiimote.

Saving and loading let you choose two kinds of save files. SRAM is the
normal kind of saving and loading that you have on a real gameboy. It only
saves up to the last checkpoint or savepoint in the game. Or you can save
a better way by using the emulator's special "Snapshot" feature which
saves the state of everything, exactly where you are up to. Loading a
Snapshot may erase your "SRAM (Auto)" so be careful.

From the game menu you can return to the game by pressing Home again, or by
clicking on the "Close" button in the top right. Or to quit that game and
choose a different game, click on the "Main Menu" button.

If you don't want to load ROMs from the SD card, you can go to the 
settings menu and choose where to load from. You can load from SD cards,
USB memory sticks/hard drives, DVD (if you installed DVDX), gamecube memory
cards, or from shared folders over the network (this is called SMB).

ROMs can be in ZIP files, but the ROM must be the first file in the ZIP. If
not, you will get an error. ROMs can also be in .7z files, or ordinary rom
files.

Patches can be used to colourise a monochrome gameboy game, or to translate
a game into your language, or to stop the game from needing special hardware.
Search the internet for patches. Many games have been translated by fans.
They can be in IPS or UPS format. You don't need to patch anything yourself. 
Just put the IPS or UPS file in the vbagx/roms folder along with the rom 
itself. The patch must have the same name as the rom. Patches can not be put
inside the ZIP file. If a rom is zipped, you might need to check inside the 
zip for the actual rom filename.

Colourised games still have some distortion in this version, but it is
improved from the previous version, and better than VBA-M. Some unpatched
monochrome gameboy games have built-in palettes in this emulator and will 
appear in colour.

You must not use patched versions of Boktai roms! (Except for the translation
patch for Boktai 3, which is highly recommended). The patches are for old
emulators that don't support the solar sensor. VBA GX and NO$GBA support the
solar sensor natively, and the patch will stop them from working.

You must also not use patched versions of WarioWare Twisted, Kirby's Tilt n
Tumble, or Yoshi's Universal Gravitation (Topsy Turvy). The original roms
are fully supported, and the patch will stop them from working.

#### Controls

See the website at http://www.wiibrew.org/wiki/VBA for better control
documentation, with illustrations and tables.

The default controls are...

+ = Gameboy Start Button
- = Gameboy Select Button
Home = Show emulator's game menu

Wii Remote by itself:
Hold the Wii Remote sideways.
2 = Gameboy A Button
1 = Gameboy B Button
A = Gameboy R Button
B = Gameboy L Button

Wii Remote + Nunchuk:
Hold the Nunchuk and ignore the Wii Remote.
Z = Gameboy A Button
C = Gameboy B Button

Classic Controller:
B = Gameboy A Button
Y = Gameboy B Button
R = Gameboy R Button
L = Gameboy L Button

You can configure the controls how you want from the controls menu. Different
controls will be used depending on what you have plugged into the Wii Remote.
Nunchuk means Nunchuk + Wii Remote. Gamecube controllers can
be used at the same time as Wii Remotes and all control the same player.
When configuring controls, press HOME to cancel.

But the controls you choose will be overridden for certain games if you 
choose "Match Wii Game" (or "Match Gamecube Game" on a Gamecube) and you have
the appropriate expansion plugged in. If the game does not have special Wii
controls, then the controls you chose will be used.

Gameboy and Gameboy colour games don't have L and R buttons. Those buttons
only work in Gameboy Advance games.

In addition to the controls you can configure, these other controls apply:

HOME, Escape: returns you to the emulator's game menu. Then press B to go 
to the main menu and B again to return to the game.
A+B, Spacebar, or right analog stick: fast forward
Right analog stick: zoom (if enabled)

#### Super Game Boy borders

VBA-GX has supported Super Game Boy borders since 2.3.1. You can enable this
feature in the Emulation settings on the main menu.

Borders can be loaded from two locations:
* PNG files in the borders folder (by default, /vbagx/borders)
* The game itself

Borders will only be loaded from the game itself when the emulator is running
in Super Game Boy mode, and the border setting in Emulation settings is set to
"From game (SGB only)". (You can also use the Emulation settings menu to
force SGB mode even for Game Boy Color games.) If the borders folder exists,
but no border for the game is present, the loaded Super Game Boy border will
be written to a .png file, which can be loaded later in "From .png file" mode.

In addition, if the borders folder exists but there is no border for the game,
the first border loaded from the game will be written to a PNG file so it can
be loaded in the future (even in Game Boy Color mode.) This means after you
run a game once in SGB mode, you can then use the same border in GBC mode.

If the border setting is set to "From .png file", borders will be loaded
from the borders folder. Borders can be up to 640x480 and will work for both
Game Boy (Color) and Game Boy Advance games.

For both loading and saving, the PNG filename is [TITLE].png, where [TITLE]
is the ROM title defined at 0x134 (for GB games) or 0xA0 (for GBA games). For
example, POKEMON_SFXAAXE.png will be loaded for Pokémon Silver. If no PNG file
by that name exists, VBA-GX will try loading default.png (for GB games) or
defaultgba.png (for GBA games) instead.

Since the borders are rendered along with the video output of the game, the
pixels in the border will be the same size as game pixels. This means that
a Game Boy game will appear in the middle 160x144 pixels of the border, and a
Game Boy Advance game will appear in the middle 240x160 pixels, regardless of
the resolution of the border PNG image.

#### Match Wii Controls

Special Wii controls exist for the following games:

These Zelda games can be played with Twilight Princess controls:
The Legend Of Zelda, Zelda 2, A Link To The Past, Link's Awakening (DX), 
Oracle of Ages, Oracle of Seasons, Minish Cap

These Mario games can be played with Mario Galaxy controls:
Super Mario Bros., Super Mario Bros. DX, Super Mario 2, Super Mario (2) 
Advance, Super Mario 3, Super Mario World, Yoshi's Island, 
Yoshi's Universal Gravitation (Topsy Turvy)

Mario Kart can be (sort of) played with Mario Kart wii controls, but it
doesn't work very well.

These Metroid games can be played with Metroid Prime 3 controls:
Metroid Zero Mission, Metroid 1, Metroid 2, Metroid Fusion

These Mortal Kombat games can be played with Mortal Kombat Armageddon controls:
Mortal Kombat, Mortal Kombat II, Mortal Kombat 3, Mortal Kombat 4, Mortal 
Kombat Advance, Mortal Kombat Deadly Alliance, Mortal Kombat Tournament 
Edition

These Lego games can be played with Lego Star Wars the Complete Saga 
controls:
Lego Star Wars The Video Game, Lego Star Wars The Original Trilogy

These Teenage Mutant Ninja Turtles games can be played with TMNT Wii controls:
TMNT, Teenage Mutant Ninja Turtles, Fall of the Foot Clan, Back from the Sewers,
Radical Rescue

These Harry Potter games can be played with Harry Potter and the Order of 
the Phoenix Wii controls:
Harry Potter 1, Harry Potter 1 GBC, Harry Potter 2, Harry Potter 2 GBC, 
Harry Potter 3, Harry Potter 4, Harry Potter 5

These Medal Of Honour games can be played with Medal Of Honour Wii controls:
Medal Of Honour Underground, Medal Of Honour Infiltrator

One Piece can be played with One Piece Unlimited Adventure controls.

Boktai 1, Boktai 2, Boktai 3, and Kirby's Tilt n Tumble, and WarioWare Twisted
can be played with controls designed for them.

#### Zelda, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

All Zelda games use the same controls as Twilight Princess on the Wii or 
Gamecube. You can also connect a Classic Controller to use similar controls
to the Ocarina Of Time for the Virtual Console, but with the R trigger
acting as the B button and an inventory like Twilight Princess. With nothing
plugged in to the Wii Remote, your configured controls are used instead.

#### The Wii Zelda controls are:

Swing your Wii Remote to draw or swing your sword. Press A to put your sword
away again. The 2 handed sword can't be drawn this way, and must be drawn
manually from the items menu, but you can swing it like normal.

Shake your Nunchuk to do a spin attack.

Use the Z Button to Z-Target and to draw and use your shield. While 
Z-Targetting you will sidestep in some games. If you have a Gust Jar 
equipped instead of a shield, it will be used for Z-Targetting.

Use the A Button to perform an action, such as rolling, talking to people, 
reading signs, picking things up, throwing things, shrinking or growing, 
pulling things, etc. It will also put away your sword or shield. In Zelda 2, 
it will jump.

Use the C Button to fast forward. It was originally the camera button in
Twilight Princess.

Press the B Button to use the currently selected item. 3 other items will be
mapped to Left, Down, and Right D-Pad buttons. Swap the currently selected
item with one of those items by pressing that D-Pad button. The three slots
correspond to the first 3 slots in your inventory. In Minish Cap, the D-Pad
buttons use the item directly instead of swapping it with the B Button, and
the B Button is the same as the down button. In Minish Cap the left item is
always the Kinstones and the down and right items correspond to the B and A
slots.

Up on the D-Pad talks to Midna, or to your hat. It will take you to the save
screen in Link's Awakening, or to the secondary items screen in the Oracle
games.

The 1 Button goes to the Map screen.
The - Button goes to the Items screen.
The + Button goes to the Quest Status screen

On the Items screen, choose an item and then press either the B Button or the
D-Pad button to move it to that slot. The change may not be visible until you
go to another screen and back. In Link's Awakening you can toggle Bomb 
Arrows by choosing the bombs and pressing Z. It will rumble for a short time
when bomb arrows are deactivated, and for a long time when bomb arrows are
activated. You still need to equip the bow to use bomb arrows. In Minish Cap
you should be able to use the IR pointer function to select items.

#### The Gamecube controller Zelda controls are:

B is the sword button. Use it to draw or swing your sword. Hold B for a spin
attack. Press A to put the sword away again. The 2 handed sword can't be
drawn this way, and must be selected manually from the items screen, but can
be swung with this (or any other) button.

Use the L Trigger to L-Target and to draw and use your shield. While 
L-Targetting you will sidestep in some games. If you have a Gust Jar 
equipped instead of a shield, it will be used for L-Targetting.

Use the A Button to perform an action, such as rolling, talking to people, 
reading signs, picking things up, throwing things, shrinking or growing, 
etc. It will also put away your sword or shield. In Zelda 2, it will jump.

Use the R Trigger to pull on blocks or walls, or to lift things. You must
have a bracelet or gloves to lift some objects. The bracelet or gloves will
be equipped automatically. This feature is unique to the Gamecube controller.

Use the right analog stick to fast forward. It was originally the camera 
control in Twilight Princess.

Press the X or Y buttons to use the two equipped items. These two items both
share the B slot, except in Minish Cap where one is in the A slot. The item
that was not used last will be in the first slot in your inventory.

Right on the D-Pad takes you to the map.
Up on the D-Pad takes you to the items screen.
Start takes you to the quest status screen.

The Z trigger talks to Midna, or to your hat. It will take you to the save
screen in Link's Awakening, or to the secondary items screen in the Oracle
games.

#### The Classic controller Zelda controls are:

B is the sword button. Use it to draw or swing your sword. Hold B for a spin
attack. Press A to put the sword away again. The 2 handed sword can't be
drawn this way, and must be selected manually from the items screen, but can
be swung with this (or any other) button.

Use the L Trigger to L-Target and to draw and use your shield. While 
L-Targetting you will sidestep in some games. If you have a Gust Jar 
equipped instead of a shield, it will be used for L-Targetting.

Use the A Button to perform an action, such as rolling, talking to people, 
reading signs, picking things up, throwing things, shrinking or growing, 
pulling, etc. It will also put away your sword or shield. In Zelda 2, it 
will jump.

Use the ZL Button to fast forward. 

Press the R Button to use the currently selected item. 3 other items will be
mapped to Left, Down, and Right on the right analog stick. They are also
mapped to ZR, Y, and X. Swap the currently selected item with one of those 
items by pressing that button or direction. The three slots correspond to 
the first 3 slots in your inventory. In Minish Cap, the D-Pad buttons use the
item directly instead of swapping it with the B Button, and the B Button is 
the same as the down button. In Minish Cap the left item is always the 
Kinstones and the down and right items correspond to the B and A slots.

+ (Start) takes you to the subscreens.
- (Select) takes you to the map or changes subscreens.

Up on the analog stick talks to Midna, or to your hat.


#### Mario, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

All Mario or Yoshi games use the same controls as Super Mario Galaxy on the 
Wii. You can also connect a Classic Controller to use similar controls to 
Super Mario World on the SNES.

#### The Wii Mario controls are:

Shake the Wii Remote to do a spin attack, or to shoot fireballs when you are
fire Mario. In some games that have a spin attack, you will need to use the
B Button instead to shoot fireballs. You can also dismount Yoshi by shaking.

Walk by moving the joystick a little, run by moving the joystick a lot.

A = jump
B = shoot, run, hold on to things, yoshi's tongue, etc.
Z = crouch or lay egg. Press Z while in the air to butt stomp.
C = camera. Hold C to look around with the joystick.
D-Pad = look around, or walk in some games
+ = pause
1 = throw egg if you are Yoshi

#### The Classic Controller Mario controls are:

Walk by moving the joystick a little, run by moving the joystick a lot.

B = jump
A = spin attack
X/Y = shoot, run, hold on to things, yoshi's tongue, etc.
ZL or sometimes L = crouch or lay egg. Press in the air to butt stomp.
+ = pause
L/R = look around (if the game supports it)
ZR = fast forward (8-bit Game Boy only)

In Super Mario World and Super Mario Land 2, you can use the A or R
buttons for a spin jump.

#### Yoshi's Universal Gravitation (Topsy Turvy), Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

The controls are the same as all other Mario or Yoshi games, except that
tilting the Wii Remote tilts the world and the screen. This affects
everything in the world and also how you move.

#### Metroid, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

All Metroid games use the same controls as Metroid Prime 3: Corruption on 
the Wii. You aim up and down by pointing the Wii Remote up and down.

#### The Wii Metroid controls are:

Aim up and down by pointing the Wii Remote up and down.

Flick the Wii remote up while in Morph Ball to spring jump.

A = shoot
B = jump
Down on D-Pad = fire missile
C = toggle Morph Ball
- = start
+ = toggle super missiles
1 = map
2 = hint

#### TMNT, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

The TMNT games (except Battle Nexus) use the same controls as TMNT on Wii,
or GameCube. With a Classic Controller they use the same controls as on
the Playstation version.

#### The Wii TMNT controls are:

Shake the Wii Remote to attack or to throw away a weapon if 
in the air. Also shake to pick up a weapon.

Shake the Nunchuk to do a spin kick.

A = jump
B = swap turtle, or charge attack
B while pointing up = super family move
C = roll
Z = special move

#### Boktai, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

The 3 Boktai games use special controls that I created. They are not based on
anything, since the real game uses a solar sensor.

The controls are the same with or without a Nunchuk.

#### The Wii Boktai controls are:

Point your Wii Remote at the sky to quickly charge your Gun Del Sol. Point
your Wii Remote at the ground to block the sunlight and prevent it from
charging or overheating. Or hold it like normal to use it like normal.

Press Home to set the real life weather in the emulator's game menu. Note
that if it is night time in real life, there will not be any sun, regardless
of what you set the weather to. Please set the weather honestly or it spoils
the fun. Note that maximum sun is not actually the best, since it rots fruit,
and overheats your gun. The weather must be set each time you play, it is not
saved.

Swing your Wii Remote to swing your sword or other weapon, if you have one.

D-Pad or Nunchuk joystick walks.

Press B to fire your Gun Del Sol.

A = read signs, open chests, talk to people
C or 2 = look around, or change subscreen (R)
+ = start
- = select
Z or 1 = change element, or change subscreen (L)
1 (if Nunchuk plugged in) = fast forward

#### WarioWare Twisted, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

WarioWare Twisted uses similar controls to the Gameboy game.

#### The Wii WarioWare Twisted controls are:

Rotate the Wii Remote to rotate.

Hold Z to lock the current menu item.

A = Select
B = Cancel
+ = Start

#### Kirby's Tilt n Tumble, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

Kirby's Tilt n Tumble uses similar controls to the Gameboy game.

#### The Kirby Tilt n Tumble controls are:

Tilt the Wii Remote to tilt the world. Shake the Wii Remote to flick Kirby
and the monsters up into the air.

A = shoot yourself out of holes in the ground, or jump from clouds.

#### Mortal Kombat, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

All Mortal Kombat games use the same controls as Mortal Kombat Armaggedon
for the Wii, except that special moves gestures are not implemented yet.

#### The Mortal Kombat Wii controls are:

Use the Nunchuk joystick to move and jump.

D-Pad left = Low Punch
D-Pad up = High Punch
D-Pad down = Low Kick
D-Pad right = High Kick
Z = block
A = throw
C = change style, run, change costume or character
+ = pause
- = change costume or character

#### Lego Star Wars, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

Both Lego Star Wars games use the same controls as Lego Star Wars: The 
Complete Saga for the Wii.

#### The Lego Star Wars Wii controls are:

Swing the Wii Remote to swing your lightsaber.
Flick the Wii Remote up to grapple.

A = Jump
B = Shoot
Z = Use the force, build lego
C = Change characters, talk to people
- = force power, special ability
+ = start
1/2 = fast forward

#### Harry Potter, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

All the Harry Potter games use the same controls as Harry Potter & The Order
Of The Phoenix on the Wii. Spell gestures are not supported yet.

#### The Harry Potter Wii controls are:

Wave the Wii Remote to cast a spell.
Nunchuk joystick walks. 
D-Pad changes subscreen in the map and navigates menu.
In Harry Potter and the Order of the Phoenix you must use the IR Pointer
to select where to cast a spell.

A = Talk, open door, push button, interract, etc. / Jinx
B = Use your wand / charm / cancel
Z = run (fast forward) / sneak
C = show location name / flute / jump
- = Maurauders map / Tasks
+ = pause / menu
1/2 = change spells

#### Medal of Honour, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

All the Medal of Honour games use the same controls as various 
Medal of Honour games and modes on the Wii.

#### The Medal of Honour Wii controls are:

In Medal of Honour Underground you turn by aiming with the Wii Remote IR
pointer on the screen like any FPS game. In Medal of Honour Infiltrator,
you don't.

Swing the Wiimote up to reload.
Move with the Nunchuk joystick.

B = shoot
- = use
+ = pause, objectives, menu
2 / D-Pad Up = reload
D-Pad Left/Right = change weapons
D-Pad Down = toggle crouch
C = strafe
1 = run

#### One Piece, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

One Piece uses the same controls as One Piece Unlimited Adventure on the Wii
or One Piece Grand Adventure (and others) on the Gamecube.

#### The One Piece Wii controls are:

A = attack
B = jump
- = change character
+ = pause
C = dash (double click and hold)
Z = grab
2 = fast forward
1 = select (maybe does nothing)

#### The One Piece Gamecube controls are:

A = attack
X = attack up
Y = jump
B = grab
R Trigger = change character
start = pause
L Trigger = dash (double click and hold)
Z = grab
right analog stick = fast forward
1 = select (maybe does nothing)

#### Kid Dracula, Match Wii Controls

Turn "Match Wii Controls" ON to use these controls.

(There's no Kid Dracula game for the Wii, but this is a good opportunity to
show off some fancy memory-swapping tricks. -libertyernie)

#### The Kid Dracula Wii controls (remote + nunchuk) are:

A = jump
B = use selected weapon
Z = use NOR weapon (fireball)
C = use BAT weapon (turn into bat for 5 sec)
+ = pause
- = switch item
1/2 = fast forward

#### The Kid Dracula Classic Controller controls are:

A/B = jump
Y = use selected weapon
X = use NOR weapon (fireball)
R/ZR = use BAT weapon (turn into bat for 5 sec)
+ = pause
- = switch item
1/2 = fast forward

In Kid Dracula, pressing the "fire" button will always shoot a small fireball,
but holding the button for a second or so to charge up the shoot lets you use
whichever item is selected.

Pressing B (nunchuk) or Y (classic) will switch back to whatever item was
selected before you pressed Z/C (nunchuk) or X/R/ZR (classic), unless you have
switched items since then.


## CREDITS

			Coding & menu design		Tantric
			Codebase update & Goomba	libertyernie
			Menu screenshots			cebolleto
			GBA tiled rendering			bgK (for RetroArch)
			Additional coding			Carl Kenner, dancinninjac
			Menu artwork				the3seashells
			Menu sound					Peter de Man
                      
			VBA GameCube/Wii			SoftDev, emukidid

			Visual Boy Advance - M		VBA-M Team
			Visual Boy Advance			Forgotten
			libogc/devkitPPC			shagkur & wintermute
			FreeTypeGX					Armin Tamzarian

			And many others who have contributed over the years!
 
 
## LINKS
 
                                  VBAGX Web Site
                          https://github.com/dborth/vbagx
 

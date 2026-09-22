# Visual Boy Advance GX Changelog

All notable changes to Visual Boy Advance GX are recorded here, newest first. For the current features and setup instructions, see [README.md](README.md).

## 3.0.2 — August 12, 2026
* Fixed crash returning to the menu when set to non-English language
* Fixed bug with Monochrome Screen setting not being applied consistently
* Improved game compatibility with a new mechanism to automatically detect the proper settings
* Fixed bugs with border handling code (refactored/rewritten)

## 3.0.1 — August 11, 2026
* Implemented a brand new Dynamic Recompilation (JIT) core for GBA games on both Wii and GameCube, built entirely from scratch. This is a from-the-ground-up addition, not a port - real GBA titles now run with plenty of headroom to run at full speed, well beyond what the interpreter core could sustain. Enable it from Settings > Emulation
* Replaced GameCube's old ROM paging system with a new ARAM/SD hybrid virtual memory pager - ROM data now streams transparently from SD into ARAM and MEM1 on demand instead of needing to fit entirely in memory ahead of time, allowing the JIT to be possible (since it doesn't have to be aware of backing data location)
* Rewritten memory management for both Wii/GameCube, freeing up 8MB+ for a JIT Cache, while still allowing 32MB ROMs
* Added cheat code support for both GBA and GB/GBC games, using the Libretro .cht file format
* GB/GBA audio is cleaner and truer to the original hardware, with one less resampling step - it is now generated natively at 48kHz - instead of upsampled from 44100Hz (GBA) and 22050Hz (GB)
* Audio samples are now written directly into the output buffer with no intermediate mixing buffer in between, reducing audio latency
* Smart dynamic audio rate control keeps playback speed correctly matched to real GBA hardware timing, with a stronger correction kicking in only when actually needed to avoid a dropout - this means fewer, less noticeable pitch adjustments during normal play
* Buffer underruns (audio momentarily running dry) now fade smoothly to silence and back instead of producing a hard click, and startup/resume is primed to avoid an initial stutter
* Reworked frameskip and frame pacing so video timing is smoother and more consistent, especially when the JIT core is running well above 60fps, and skipped frames are spaced more evenly instead of clumping
* Added FPS display option

## 3.0.0 — July 6, 2026
* Added video filters - hq2x, Scale2x, Scanlines, 2xBR, DDT
* Optimized video rendering
* Replaced C texture generation with optimized PPC ASM
* Improved audio code
* Refactored/improved synchronization and frameskip handling
* Numerous VBA-M core performance optimizations
* New blur effect when pausing a game
* Rewritten in-game cursor
* Reworked save/load device and preferences logic
* Fixed crash when removing devices (eg: SD/USB)
* Fixed flashes/artifacts/colors when switching video modes
* Streamlined/enhanced build
* General performance enhancements
* Other general enhancements
* Compiled with latest devkitPPC/libogc2

## 2.5.1 — April 13, 2026
* Compiled with latest devkitPPC/libogc2

## 2.5.0 — July 30, 2025
* Added GC Loader support (mrysav)
* Compiled with latest devkitPPC/libogc2

## 2.4.9 — May 18, 2025
* Compiled with latest devkitPPC/libogc2
* Updated MBC2 save handling (saulfabregwiivc)
* Increased max zoom to 1.6 for GB/GBA

## 2.4.8 — March 30, 2024
* Added L+R+START for return to the menu for GCN controller (saulfabreg)
* Fixed MBC2 data saving for F-1 Race, Kirby's Pinball Land, etc. (saulfabreg, based on fix from Steelskin)
* Fixed MBC7 data saving for Kirby Tilt 'n' Tumble (saulfabreg, based on fix from Steelskin)
* Compiled with latest devkitPPC/libogc
* Added Swedish translation (IsakTheHacker)
* Updated translations

## 2.4.7 — July 31, 2023
* Compiled with latest devkitPPC/libogc
* Switch to chosen video mode on first load if not automatic
* Fixed a crash upon relaunching after removing a SD/USB device (InfiniteBlueGX)

## 2.4.6 — June 15, 2022
* Compiled with latest devkitPPC/libogc
* Added "Enable Turbo Mode" toggle to the Video Settings menu (based on InfiniteBlueGX's code)
* Updated translations
* Improved forwarder support

## 2.4.5 — March 23, 2021
* Added L+R+START for back to menu for Wii Classic Controller
* Updated French translation (thanks Tanooki16!)
* Fixed issue with displaying screenshots

## 2.4.4 — February 6, 2021
* Fixed SD2SP2 / SD gecko issues (again)

## 2.4.3 — January 31, 2021
* Fixed SD2SP2 issues
* Changed max game image dimensions to 640x480 to support screenshots

## 2.4.2 — January 18, 2021
* Compiled with latest devkitPPC/libogc
* Added ability to change the player mapped to a connected controller
* Significant memory usage reductions (fonts and loading cover images)
* Other minor fixes

## 2.4.1 — June 29, 2020
* Compiled with latest devkitPPC/libogc
* Fixed some 3rd party controllers with invalid calibration data
* Translation updates
* Added Wii U vWii Channel, widescreen patch, and now reports console/CPU speed
* Added support for serial port 2 (SP2 / SD2SP2) on Gamecube
* Fixed Wii U Pro controller button mapping not being used in one case
* Fixed ZL button mapping for Wii U GamePad
* Other minor fixes

## 2.4.0 — April 13, 2019
* Fixed crash when used as wiiflow plugin
* Fixed crash on launch when using network shares
* Fixed issues with on-screen keyboard
* Updated Korean translation

## 2.3.9 — January 25, 2019
* Added ability to load external fonts and activated Japanese/Korean translations. Simply put the ko.ttf or jp.ttf in the app directory
* Added ability to customize background music. Simply put a bg_music.ogg in the app directory
* Added ability to change preview image source with + button (thanks Zalo!)
* Fixed issue with resetting motion controls
* Fixed issue with Mode 0 graphics transparency

## 2.3.8 — January 4, 2019
* Restored changes lost from 2.3.0 core upgrade (GameCube virtual memory, optimizations from dancinninjac, GB color palettes, rotation/tilt for WarioWare Twisted, in-game rumble)
* Improved WiiFlow integration
* Fixed controllers with no analog sticks
* Added Wii U GamePad support (thanks Fix94!)

## 2.3.7 — August 28, 2018
* Allow loader to pass two arguments instead of three (libertyernie)
* don't reset settings when going back to an older version
* Fix a few potential crashes caused by the GUI
* Other minor fixes/improvements
* Compiled with latest libOGC/devkitPPC

## 2.3.6 — December 11, 2016
* Restored Wiiflow mode plugin by fix94
* Restored fix filebrowser window overlapping
* Change all files End Of Line to windows mode
* Remove update check for updates

## 2.3.5 — December 10, 2016
* Hide saving dialog that pops up briefly when returning from a game

## 2.3.4 — September 15, 2016
* Added the delete save file (SRAM / Snapshot) option
* Changed the box colors for the SRAM and Snapshots files to match the color scheme of the emu GUI
* Change the "Power off Wii" exit option to completely turn off the wii, ignoring the WC24 settings
* Updated settings file name in order to have it's own settings file name
* Added an option to switch between screenshots, covers, or artwork images, with their respective named folders at the device's root. You can set which one to show, by going to Settings > Menu > Preview Image. The .PNG image file needs to have the same name as the ROM (e.g.: Mother 3.png)
* Removed sound from GUI (thanks to Askot)
* Added option to switch between the Green or Monochrome GB color screen. You can set which one to show by going to Settings > Emulation > GB Screen Palette

## 2.3.3 — June 25, 2016
* Fixed the GC pad Down input on the File browser window
* Added Koston's green gb color screen
* Added the Screenshot Button
* Increased and Centered the Screenshot image and reduce game list width
* Added a background for the preview image
* Added the WiiuPro Controller icon on the controller settings
* Fix DSI error / Bug from Emulator Main Menu

## 2.3.2 — March 4, 2015 — libertyernie
* Wii U: if widescreen is enabled in the Wii U setting, VBA GX will use a 16:9 aspect ratio, except while playing a game with fixed pixel mode turned on
* There are now three options for border in the emulation settings menu (see "Super Game Boy borders" section for details)
* PNG borders now supported for GBA games
* Video mode "PAL (50Hz)" renamed to "PAL (576i)"
* Video mode "PAL (60Hz)" renamed to "European RGB (480i)"
* 240p support added (NTSC and European RGB modes)
* All video modes now use a width of 704 for the best pixel aspect ratio

## 2.3.1b — November 8, 2014 — Glitch
* Added FIX94's libwupc for WiiU Pro Controllers
* Added tueidj's vWii Widescreen Fix

## 2.3.1 — October 14, 2014 — libertyernie
* Super Game Boy border support
* Borders can be loaded from (and are automatically saved to) PNG files
* Any border loaded from the game itself will override the custom PNG border
* Custom palette support from 2.2.8 restored
* Option added to select Game Boy hardware (GB/SGB/GBC/auto)
* Fixed pixel ratio mode added
* Overrides zoom and aspect ratio settings
* To squish the picture so it appears correctly on a 16:9 TV, you can open the settings.xml file and add 10 to the gbFixed/gbaFixed value. However, setting your TV to 4:3 mode will yield a better picture.
* Real-time clock fixes for GB/GBC games, including Pokémon G/S/C
* RTC data in save file stored as little-endian
* Option added for UTC offset in the main menu (only required if you use the same SRAM on other, time-zone-aware platforms)
* New option for selecting "sharp" or "soft" filtering settings
* "Sharp" was the default for 480p, "soft" was the default for 480i

## 2.3.0 — September 10, 2014 — libertyernie
* VBA-M core updated to r1231
* Tiled rendering used for GBA games (new VBA-M feature, originally from RetroArch) - provides a major speed boost!
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
* Any Game Boy ROM stored within a Goomba ROM can be loaded "natively" in the Game Boy (Color) emulator (or the Goomba ROM can be loaded as GBA)
* Game Boy SRAM stored within Goomba SRAM is loaded and saved correctly

## 2.2.8 — July 29, 2012
* Fixed lag with GameCube controllers

## 2.2.7 — July 7, 2012
* Fixed PAL support

## 2.2.6 — July 6, 2012
* Support for newer Wiimotes
* Fixed missing audio channel bug (eg: in Mario & Luigi: Superstar Saga)
* Improved controller behavior - allow two directions to be pressed simultaneously
* Compiled with devkitPPC r26 and libogc 1.8.11

## 2.2.5 — May 15, 2011
* Added Turkish translation

## 2.2.4 — March 23, 2011
* Fixed browser regressions with stability and speed

## 2.2.3 — March 19, 2011
* Improved USB and controller compatibility (recompiled with latest libogc)
* Enabled SMB on GameCube (thanks Extrems!)
* Added Catalan translation
* Translation updates

## 2.2.2 — October 7, 2010
* Fixed "blank listing" issue for SMB
* Improved USB compatibility and speed
* Added Portuguese and Brazilian Portuguese translations
* Channel updated (improved USB compatibility)
* Other minor changes

## 2.2.1 — August 14, 2010
* IOS 202 support removed
* USB 2.0 support via IOS 58 added - requires that IOS58 be pre-installed
* DVD support via AHBPROT - requires latest HBC

## 2.2.0 — July 22, 2010
* Fixed broken auto-update

## 2.1.9 — July 20, 2010
* Reverted USB2 changes

## 2.1.8 — July 14, 2010
* Ability to use both USB ports (requires updated IOS 202 - WARNING: older versions of IOS 202 are NO LONGER supported)
* Hide non-ROM files
* Other minor improvements

## 2.1.7 — June 20, 2010
* USB improvements
* GameCube improvements - audio, SD Gecko, show thumbnails for saves
* Other minor changes

## 2.1.6 — May 19, 2010
* DVD support fixed
* Fixed some potential hangs when returning to menu
* Video/audio code changes
* Fixed scrolling text bug
* Other minor changes

## 2.1.5 — April 9, 2010
* Fix auto-save bug

## 2.1.4 — April 9, 2010
* Fixed issue with saves (GBA) and snapshots (GB)
* Most 3rd party controllers should work now (you're welcome!)
* Translation updates (German and Dutch)
* Other minor changes

## 2.1.3 — March 30, 2010
* Fixed ROM allocation. Should solve some unexplained crashes
* Numerous performance optimizations (thanks dancinninja!)
* DVD / USB 2.0 support via IOS 202. DVDx support has been dropped. It is highly recommended to install IOS 202 via the included installer
* Multi-language support (only French translation is fully complete)
* Thank you to everyone who submitted translations
* SMB improvements/bug fixes
* Minor video & input performance optimizations
* Disabling rumble now also disables in-game rumbling
* Fixed saving of GB screen position adjustment

## 2.1.2 — December 23, 2009
* Numerous core optimizations (thanks dancinninjac!)
* File browser now scrolls down to the last game when returning to browser
* Auto update for those using USB now works
* Fixed scrollbar up/down buttons
* Minor optimizations

## 2.1.1 — December 7, 2009
* Save state corruption issues fixed

## 2.1.0 — December 2, 2009
* Fixed SMB (for real this time!)

## 2.0.9 — November 30, 2009
* Fixed SMB
* Added separate horizontal/vertical zoom options, and separate GB/GBA ones
* Improved scrolling timing - the more you scroll, the fast it goes
* Fixed reset button on Wii console - now you can reset multiple times
* APU optimization (dancinninjac)
* Minor code optimizations
* Reduce memory fragmentation - fixes out of memory crashes

## 2.0.8 — October 7, 2009
* Revamped filebrowser and file I/O
* Fixed MBC2 saving/loading
* Fixed some GB-Z80 instructions
* DVD loading in GameCube should work now (untested and unsupported)
* Many, many other bug fixes

## 2.0.7 — September 16, 2009
* Text rendering corrections
* SMB improvements
* Built with latest libraries
* Video mode switching now works properly
* Other minor bugfixes and cleanup

## 2.0.6 — July 22, 2009
* Fixed "No game saves found." message when there are actually saves.
* Fixed shift key on keyboard
* Text scrolling works again
* Change default prompt window selection to "Cancel" button

## 2.0.5 — July 9, 2009
* Faster SMB/USB browsing
* Last browsed folder is now remembered
* Fixed controller mapping reset button
* Fixed no sound on GameCube version
* Directory names are no longer altered
* Preferences now only saved on exit
* Fixed on-screen keyboard glitches
* SRAM auto-saved on power-off from within a game
* Prevent 7z lockups, better 7z error messages

## 2.0.4 — June 30, 2009
* Fixed auto-update
* Increased file browser listing to 10 entries, decreased font size
* Added text scrolling on file browser
* Added reset button for controller mappings
* Settings are now loaded from USB when loading the app from USB on HBC
* Fixed menu crashes caused by ogg player bugs
* Fixed memory card saving verification bug
* Fixed game savebrowser bugs
* Miscellaneous code cleanup/corrections

## 2.0.3 — May 30, 2009
* Fixed SD/USB corruption bug
* SMB works again
* GUI bugs fixed, GUI behavioral improvements
* GB Palette editing
* More built-in palettes
* Palettes now fade to white correctly instead of getting brighter
* Can now turn off palette colorizing
* Workaround for palette issue on Mega Man I GB - palette disabled
* Star Wars, TMNT, Lord Of The Rings, Castlevania Wii Controls
* Fix for WarioWare startup - Nunchuk C button or Wii Remote B button will now make calibration easy by locking the gyroscope.
* Fixed issues with constant rumbling

## 2.0.2 — May 26, 2009
* Improved stability
* Fixed broken SDHC from HBC 1.0.2 update
* Fixed issues with returning to menu from in-game
* Add option to disable rumble
* Auto-determines if HBC is present - returns to Wii menu otherwise
* Unfiltered mode fixed
* Miscellaneous bugfixes

## 2.0.1 — April 30, 2009
* Multiple state saves now working
* Built with more stable libogc/libfat
* Fixed settings saving glitches
* Fixed Mortal Kombat GameCube controller bug
* Fixed Zelda DX palette bug
* Fixed Harry Potter 1-3 keyboard bug

## 2.0.0 — April 27, 2009
* New GX-based menu, with a completely redesigned layout. Has Wiimote IR support, sounds, graphics, animation effects, and more
* Thanks to the3seashells for designing some top-notch artwork, to Peter de Man for composing the music, and a special thanks to shagkur for fixing libogc bugs that would have otherwise prevented the release
* Onscreen keyboard for changing save/load folders and network settings
* Menu configuration options (configurable exit button, wiimote orientation, volumes)
* New save manager, allowing multiple saves and save browsing. Shows screenshots for Snapshot saves, and save dates/times
* Added video shifting option
* Added video mode selection (recommended to leave on Automatic)
* USB Mouse support (buttons only)
* Keyboard shift key bug fixed
* Built-in 14 colour palettes for some monochrome gameboy games (Magnetic Soccer, Malibu Beach Volleyball, Marble Madness, Metroid 2, Mortal Kombat, Mortal Kombat II, Mortal Kombat 3, Mr. Do!)
* Rumble works in GBC games designed for rumble cartridges but shipped without rumble cartridges, such as Disney's Tarzan for GBC
* Improved Mortal Kombat Wii Controls
* Mortal Kombat games now have many extra characters to choose
* Wii Controls for more Teenage Mutant Ninja Turtles games
* Improved Lego Star Wars controls
* Boktai menu now tells you when there can't be sun because it is night
* Minor bug fixes

## 1.0.9 — April 7, 2009
* Gamecube controller should no longer rumble constantly

## 1.0.8 — April 4, 2009
* "Match Wii Game" controls option! Games that have a Wii equivalent can be played using the controls for that Wii game. For example all Zelda games can be played with Twilight Princess controls. See the Instructions section below for important details.
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

## 1.0.7 — January 27, 2009
* Updated to VBA-M r847
* Corrected sound interpolation
* Faster SD/USB - new read-ahead cache
* Removed trigger of back to menu for Classic Controller right joystick
* Fixed a bug with reading files < 2048 bytes
* Fixed GBA games on GameCube
* Fixed homebrew GBA games on GameCube
* Fixed some memory leaks, buffer overflows, etc
* Code cleanup, other general bugfixes

## 1.0.6 — December 24, 2008
* Fixed save state saving bug
* Fixed unstable SD card access
* Proper SD/USB hotswap (Wii only)
* Auto-update feature (Wii only)
* Rewritten SMB access - speed boost, NTLM now supported (Wii only)
* Improved file access code
* Resetting preferences now resets controls
* Minor bug fixes

## 1.0.5 — November 19, 2008
* SDHC works now
* Frameskipping tweaks
* Fixed snapshot loading issue
* Full widescreen support
* Changed scaling
* Zooming fixed (thanks eke-eke!)
* PAL timing changes - EURGB60 mode forced
* Wii - Added console/remote power button support
* Wii - Added reset button support (resets game)
* Wii - Settings file is now named settings.xml and is stored in the same folder as the DOL (eg: apps/vbagx/settings.xml)
* GameCube - Added DVD motor off option
* GameCube - Fixed GBA loading issue

## 1.0.4 — October 28, 2008
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

## 1.0.3 — October 15, 2008
* New timing / frameskip algorithm - should (hopefully) work 100% better!
* Performance improvements - video threading, PPC core partly activated
* Video zooming option
* Unfiltered video option
* 7z support
* Loading progress bars added

## 1.0.2 — October 6, 2008
* New core! The core is now a custom combination of VBA-M and VBA 1.72
* Added DVD, SMB, ZIP, GameCube MC support
* Faster USB/SD speeds
* Screen alignment and flickering problems fixed
* 128K save support added
* Better emulation speeds. Should now be nearly full speed all the time for most games.
* Turbo speed feature. Mapped to right C-stick (classic controller & Gamecube controller), and A+B for wiimote
* Controller mapping preferences bug fixed. Your preferences will reset automatically to correct any problems in your preferences file
* Many other tweaks behind the scenes

## 1.0.1 — September 18, 2008
* GBA games now run at full speed
* Menu improvements, with spiffy new background
* Fixed L/R buttons - they work now

## 1.0.0 — September 16, 2008
* Now compiles with devkitpro r15
* One makefile to make all versions
* Complete rewrite based on code from SNES9x GX
* Now has a menu! ROM selector, preferences, controller mapping, etc
* Wiimote, Nunchuk, and Classic controller support
* Button mapping for all controller types
* Full support for SD and USB
* Load/save preference selector. ROMs, saves, and preferences are saved/loaded according to these
* 'Auto' settings for save/load - attempts to automatically determine your load/save device(s) - SD, USB
* Preferences are loaded and saved in XML format. You can open VBAGX.xml edit all settings, including some not available within the program

¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
                            - Visual Boy Advance GX -
                                  Version 1.0.7
                         http://code.google.com/p/vba-wii   
                               (Under GPL License)
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
Visual Boy Advance GX is a modified port of VBA-M.
With it you can play GBA/Game Boy Color/Game Boy games on your Wii/GameCube.

-=[ Features ]=-

* Wiimote, Nunchuk, Classic, and Gamecube controller support
* SRAM and State saving
* IPS/UPS/PPF patch support
* Custom controller configurations
* SD, USB, DVD, SMB, GC Memory Card, Zip, and 7z support
* Compatiblity based on VBA-M r847
* MEM2 ROM Storage for fast access
* Auto frame skip for those core heavy games
* Turbo speed, video zooming, widescreen, and unfiltered video options

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                         UPDATE HISTORY                        ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

[What's New 1.0.7 - January 27, 2009]
* Updated to VBA-M r847
* Corrected sound interpolation
* Faster SD/USB - new read-ahead cache
* Removed trigger of back to menu for Classic Controller right joystick
* Fixed a bug with reading files < 2048 bytes
* Fixed GBA games on GameCube
* Fixed homebrew GBA games on GameCube
* Fixed some memory leaks, buffer overflows, etc
* Code cleanup, other general bugfixes

[What's New 1.0.6 - December 24, 2008]
* Fixed save state saving bug
* Fixed unstable SD card access
* Proper SD/USB hotswap (Wii only)
* Auto-update feature (Wii only)
* Rewritten SMB access - speed boost, NTLM now supported (Wii only)
* Improved file access code
* Resetting preferences now resets controls
* Minor bug fixes

[What's New 1.0.5 - November 19, 2008]
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

[What's New 1.0.4 - October 28, 2008]
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

[What's New 1.0.3 - October 15, 2008]
* New timing / frameskip algorithm - should (hopefully) work 100% better!
* Performance improvements - video threading, PPC core partly activated
* Video zooming option
* Unfiltered video option
* 7z support
* Loading progress bars added

[What's New 1.0.2 - October 6, 2008]
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

[What's New 1.0.1 - September 18, 2008]
* GBA games now run at full speed
* Menu improvements, with spiffy new background - thanks brakken!
* Fixed L/R buttons - they work now

[What's New 1.0.0 - September 16, 2008]

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

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                         SETUP & INSTALLATION                  ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

Unzip the archive. You will find the following folders inside:

apps			Contains Homebrew Channel ready files
				(see Homebrew Channel instructions below)
				
gamecube		Contains GameCube DOL file (not required for Wii)
				
vbagx			Contains the directory structure required for storing
				roms and saves. By default, roms are loaded from 
				"vbagx/roms/" and saves / preferences are stored in 
				"vbagx/saves/".
				
-=[ Loading / Running the Emulator ]=-

Wii - Via Homebrew Channel:
--------------------------------
The most popular method of running homebrew on the Wii is through the Homebrew
Channel. If you already have the channel installed, just copy over the apps folder
included in the archive into the root of your SD card.

Remember to also create the vbagx directory structure required. See above.

If you haven't installed the Homebrew Channel yet, read about how to here:
http://hbc.hackmii.com/

Gamecube:
--------------------------------
You can load VBAGX via sdload and an SD card in slot A, or by streaming 
it to your Gamecube, or by booting a bootable DVD with VBAGX on it. 
This document doesn't cover how to do any of that. A good source for information 
on these topics is the tehskeen forums: http://www.tehskeen.com/forums/
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
-=[ Credits ]=-

Visual Boy Advance GX             Tantric
GameCube/Wii Port Improvements    emukidid
Original GameCube Port            SoftDev
Visual Boy Advance 1.7.2          Forgotten
libogc                            Shagkur & wintermute
Testing                           tehskeen users

And many others who have contributed over the years!
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
                                  VBAGX Web Site
                          http://code.google.com/p/vba-wii
                              
                              TehSkeen Support Forums
                              http://www.tehskeen.net
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤

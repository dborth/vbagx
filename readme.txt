¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
                            - Visual Boy Advance GX -
                                  Version 1.0.2   
                               (Under GPL License)
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
-=[ Explanation ]=-
 
Visual Boy Advance GX is a port of Visual Boy Advance 1.7.2.
With it you can play GBA/Game Boy Color/Game Boy games on your Wii/GameCube.

-=[ Features ]=-

* Wiimote, Nunchuk, Classic, and Gamecube controller support
* SRAM and State saving
* Custom controller configurations
* SD and USB support

×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                         UPDATE HISTORY                        ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'

[What's New 1.0.2]
* New core! The core is a modified version of VBA-M and VBA 1.80 beta 3
* Better emulation speeds. Should now be nearly full speed all the time
* Turbo speed feature. Mapped to right C-stick (classic controller & 
  Gamecube controller), and A+B for wiimote
* Controller mapping preferences bug fixed. Your preferences will reset
  automatically to correct any problems in your preferences file
* Fix a frameskipping bug
* Some tweaks behind the scenes

[What's New 1.0.1]
* GBA games now run at full speed
* Menu improvements, with spiffy new background - thanks brakken!
* Fixed L/R buttons - they work now

[What's New 1.0.0]

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
				
executables		Contains Gamecube / Wii DOL files
				(for loading from other methods)
				
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

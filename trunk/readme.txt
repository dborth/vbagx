¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
                            - Visual Boy Advance GX -
                                  Version 2.0.6
                         http://code.google.com/p/vba-wii   
                               (Under GPL License)
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
Visual Boy Advance GX is a modified port of VBA-M.
With it you can play GBA/Game Boy Color/Game Boy games on your Wii/GameCube.

-=[ Features ]=-

* Wiimote, Nunchuk, Classic, Gamecube controller, Keyboard and Mouse support
* Rotation sensors, Solar sensors, and Rumble support
* Optional special Wii controls built-in for some games
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
This document doesn't cover how to do any of that.
 
×—–­—–­—–­—–­ –­—–­—–­—–­—–­—–­—–­—–­—–­—–­— ­—–­—–­—–­—–­—–­—–­—–­—-­—–­-–•¬
|0O×øo·                          INSTRUCTIONS                         ·oø×O0|
`¨•¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨ ¨¨¨¨¨¨¨¨¨¨¨¨¨'
Note! A USB Mouse must be plugged in before starting the emulator, or it
won't be recognised. A mouse or keyboard is not required. Keyboards might or
might not need to be unplugged and replugged to work.

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

Keyboard and mouse do not work in the menu yet. 

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

-=[ Controls ]=-

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
Use the buttons as labelled.
A = Gameboy A Button
B = Gameboy B Button
R = Gameboy R Button
L = Gameboy L Button

Keyboard:
X = Gameboy A Button
Z = Gameboy B Button
S = Gameboy R Button
A = Gameboy L Button
Enter = Gameboy Start Button
Backspace = Gameboy Select Button
Space = Fast forward

You can configure the controls how you want from the controls menu. Different
controls will be used depending on what you have plugged into the Wii Remote.
Nunchuk means Nunchuk + Wii Remote. Gamecube controllers and Keyboards can
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

-=[ Match Wii Controls ]=-

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

Boktai 1, Boktai 2, Boktai 3, Kirby's Tilt n Tumble, and WarioWare Twisted 
can be played with controls I designed for them.

-=[ Zelda, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

All Zelda games use the same controls as Twilight Princess on the Wii or 
Gamecube. You can also connect a Classic Controller to use similar controls
to the Ocarina Of Time for the Virtual Console, but with the R trigger
acting as the B button and an inventory like Twilight Princess. With nothing
plugged in to the Wii Remote, your configured controls are used instead.

The Wii Zelda controls are:
===========================

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

The Gamecube controller Zelda controls are:
===========================================

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

The Classic controller Zelda controls are:
==========================================

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


-=[ Mario, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

All Mario or Yoshi games use the same controls as Super Mario Galaxy on the 
Wii. You can also connect a Classic Controller to use similar controls to 
Super Mario World on the SNES.

The Wii Mario controls are:
===========================

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

The Classic Controller Mario controls are:
==========================================

Walk by moving the joystick a little, run by moving the joystick a lot.

B = jump
A = spin attack
X/Y = shoot, run, hold on to things, yoshi's tongue, etc.
ZL or sometimes L = crouch or lay egg. Press in the air to butt stomp.
+ = pause
L/R sometimes look around


-=[ Yoshi's Universal Gravitation (Topsy Turvy), Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

The controls are the same as all other Mario or Yoshi games, except that
tilting the Wii Remote tilts the world and the screen. This affects
everything in the world and also how you move.


-=[ Metroid, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

All Metroid games use the same controls as Metroid Prime 3: Corruption on 
the Wii. You aim up and down by pointing the Wii Remote up and down.

The Wii Metroid controls are:
=============================
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


-=[ TMNT, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

The TMNT games (except Battle Nexus) use the same controls as TMNT on Wii,
or GameCube. With a Classic Controller they use the same controls as on
the Playstation version.

The Wii TMNT controls are:
==========================
Shake the Wii Remote to attack or to throw away a weapon if 
in the air. Also shake to pick up a weapon.

Shake the Nunchuk to do a spin kick.

A = jump
B = swap turtle, or charge attack
B while pointing up = super family move
C = roll
Z = special move

-=[ Boktai, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

The 3 Boktai games use special controls that I created. They are not based on
anything, since the real game uses a solar sensor.

The controls are the same with or without a Nunchuk.

The Wii Boktai controls are:
============================

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

-=[ WarioWare Twisted, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

WarioWare Twisted uses similar controls to the Gameboy game.

The Wii WarioWare Twisted controls are:
=======================================

Rotate the Wii Remote to rotate.

Hold Z to lock the current menu item.

A = Select
B = Cancel
+ = Start

-=[ Kirby's Tilt n Tumble, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

Kirby's Tilt n Tumble uses similar controls to the Gameboy game.

The Kirby Tilt n Tumble controls are:
=====================================

Tilt the Wii Remote to tilt the world. Shake the Wii Remote to flick Kirby
and the monsters up into the air.

A = shoot yourself out of holes in the ground, or jump from clouds.

-=[ Mortal Kombat, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

All Mortal Kombat games use the same controls as Mortal Kombat Armaggedon
for the Wii, except that special moves gestures are not implemented yet.

The Mortal Kombat Wii controls are:
===================================

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

-=[ Lego Star Wars, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

Both Lego Star Wars games use the same controls as Lego Star Wars: The 
Complete Saga for the Wii.

The Lego Star Wars Wii controls are:
====================================
Swing the Wii Remote to swing your lightsaber.
Flick the Wii Remote up to grapple.

A = Jump
B = Shoot
Z = Use the force, build lego
C = Change characters, talk to people
- = force power, special ability
+ = start
1/2 = fast forward

-=[ Harry Potter, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

All the Harry Potter games use the same controls as Harry Potter & The Order
Of The Phoenix on the Wii. They also use the keyboard controls from the PC
version of each game. Spell gestures are not supported yet.

The Harry Potter Wii controls are:
==================================
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

-=[ Medal of Honour, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

All the Medal of Honour games use the same controls as various 
Medal of Honour games and modes on the Wii.

The Medal of Honour Wii controls are:
=====================================

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

-=[ One Piece, Match Wii Controls ]=-

Turn "Match Wii Controls" ON to use these controls.

One Piece uses the same controls as One Piece Unlimited Adventure on the Wii
or One Piece Grand Adventure (and others) on the Gamecube.

The One Piece Wii controls are:
===============================

A = attack
B = jump
- = change character
+ = pause
C = dash (double click and hold)
Z = grab
2 = fast forward
1 = select (maybe does nothing)

The One Piece Gamecube controls are:
===============================

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

¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
-=[ Credits ]=-

			Coding & menu design		Tantric
			Additional coding			Carl Kenner
			Menu artwork				the3seashells
			Menu sound					Peter de Man
                      
			¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨¨
			VBA GameCube/Wii			SoftDev, emukidid

			Visual Boy Advance - M		VBA-M Team
			Visual Boy Advance			Forgotten
			libogc/devkitPPC			shagkur & wintermute
			FreeTypeGX					Armin Tamzarian

			And many others who have contributed over the years!
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤
 
                                  VBAGX Web Site
                          http://code.google.com/p/vba-wii
 
¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤°`°¤ø,¸,ø¤°`°¤ø,¸¸,ø¤

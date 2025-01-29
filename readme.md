A8DS
--------------------------------------------------------------------------------
A8DS is an Atari 8-bit computer emulator for the DS/DSi.  It targets the 
800XL / 130XE systems and various hardware extensions to increase the memory. 
The stock 800XL had 64KB of RAM. The default A8DS configuration is an
XL/XE machine with 128KB of RAM which will run most of the 8-bit library.
Other memory configurations are available from 48K up to 1088K which is enough
to run nearly the entire 8-bit line up of games on the DS/DSi.

The emulator runs cart dumps (ROM or CAR types), executable images (XEX) or disk 
images (ATR or ATX) which are the most popular "ROM" types for emulators. The goal 
here is to make this as simple as possible - point to the 8-bit Atari game/program 
you want to run and off it goes!

![A8DS](https://github.com/wavemotion-dave/A8DS/blob/main/arm9/gfx/bgTop.png)


Features :
----------------------------------------------------------------------------------
*  Memory configurations including 48K (base ATARI400/800), 64K XL/XE, 128K XE, 320K RAMBO, 576K COMPY and 1088K RAMBO
*  CAR and ROM cartridge-based games up to 1MB in size
*  XEX Atari 8-bit Executable images of any size provided they fit into the chosen Memory Configuration
*  ATR and ATX disk-based games (two emulated drives supported as D1 and D2)
*  NTSC and PAL support
*  Virtual keyboard in various Atari 800/XL/XE stylings
*  R-Time8 Real-Time Clock support (mostly for SpartaDOS X)
*  Built in Altirra OS (3.41) and BASIC (1.58) but optional external BIOS/BASIC support to use the real Atari firmware
*  High Score support for 10 scores per game
*  Full configuration of DS keys to any Atari 8-bit joystick/key/button
*  Save and Restore state so you can snap out the memory/CPU and restore it to pick back up exactly where you left off

Optional BIOS ROMs
----------------------------------------------------------------------------------
There is a built-in Altirra BIOS (thanks to Avery Lee) which is reasonably compatible
with many games. However, a few games will require the original ATARI BIOS - and,
unfortunately, there were many variations of those BIOS over the years to support
various Atari computer models released over a span of a decade.

A8DS supports 3 optional (but highly recommended) Atari BIOS and BASIC files as follows (with their CRC32):

*  atarixl.rom  (0x1f9cd270)  - this is the 16k XL/XE version of the Atari BIOS for XL/XE Machines (NTSC Rev 02 - BB 01.02, 10.May.1983) 
*  atariosb.rom (0x3e28a1fe)  - this is the 12k Atari 800 OS-B revision BIOS for older games  (NTSC OS-B version 2)
*  ataribas.rom (0x7d684184)  - this is the 8k Atari BASIC cartridge (Rev C)
*  a5200.rom    (0x4248d3e3)  - this is the 2k Atari 5200 BIOS ROM (Rev 1)

You can use other versions of these BIOS files, but these are the ones that I'm testing/running with.

You can install zero, one or more of these files and if you want to use these real ROMs
they must reside in the same folder as the A8DS.NDS emulator or you can place your
BIOS files in /roms/bios or /data/bios) and these files must be exactly
so named as shown above. These files are loaded into memory when the emulator starts 
and remain available for the entire emulation session. Again, if you don't have a real BIOS, 
a generic but excellent one is provided from Avery Lee who made Altirra 
which is released as open-source software.  Also optional is ataribas.rom for the 8K basic 
program. If not supplied, the built-in Altirra BASIC 1.58 is used.

I've not done exhaustive testing, but in many cases I find the Altirra BIOS does a
great job as a replacement for the Atari OS/BASIC roms. However, for maximum compatibility,
it is recommended you find the above OS/BASIC roms.

Do not ask me about rom files, you will be promptly ignored. A search with Google will certainly 
help you. 

A Tale of Two Versions
----------------------------------------------------------------------------------
There are two different versions of A8DS. One is simply called A8DS.nds and this 
version will run on older DS units (such as the DS-Phat or DS-Lite) and runs in 
a faster mode without the more complex Antic and GTIA "Cycle Exact" handling. This
means that on some games and demos, there may be graphical glitches and other 
screen artifacts. The other file is called A8DSi.nds and brings in the cycle
exact timing to clean up those glitches. Only the DSi running in 2X CPU mode can
handle this extra complexity in emulation. 

Copyright:
--------------------------------------------------------------------------------
A8DS - Atari 8-bit Emulator designed to run on the DS/DSi is
Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)

Copying and distribution of this emulator, its source code and associated 
readme files, with or without modification, are permitted in any medium without 
royalty provided this full copyright notice (including the Atari800 one below) 
is used and wavemotion-dave, alekmaul (original port), Atari800 team (for the
original source) and Avery Lee (Altirra OS) are credited and thanked profusely.

The A8DS emulator is offered as-is, without any warranty.

Since much of the original codebase came from the Atari800 project, and since
that project is released under the GPL V2, this program and source must also
be distributed using that same licensing model. See COPYING for the full license
but the original Atari800 copyright notice is retained below:

> Atari800 is free software; you can redistribute it and/or modify
> it under the terms of the GNU General Public License as published by
> the Free Software Foundation; either version 2 of the License, or
> (at your option) any later version.

> Atari800 is distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
> GNU General Public License for more details.

> You should have received a copy of the GNU General Public License
> along with Atari800; if not, write to the Free Software
> Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


Credits:
--------------------------------------------------------------------------------
* Atari800 team for source code (https://atari800.github.io/)
* Altirra and Avery Lee for a kick-ass substitute BIOS, the Altirra Hardware Manual (a must read) and generally being awesome (https://www.virtualdub.org/altirra.html)
* Wintermute for devkitpro and libnds (http://www.devkitpro.org).
* Alekmaul for porting the original A5200DS of which this is heavily based.
* Darryl Hirschler for the awesome Atari 8-bit Keyboard Graphics.
* The good folks over on GBATemp and AtariAge for their support.


Additional Details :
----------------------------------------------------------------------------------
Games generally run full-speed with just a handful of exceptions - most notably on the older DS hardware 
or when running from a flash cart (R4 or similar) which won't give you access to a DSi 2X CPU mode. 
If you load a game and it doesn't load properly or you get a message at the bottom of the screen after loading - 
it usually means that the game requires some other hardware configuration to run. See the Troubleshooting
guide further below for more tips to try and get a game running. I'll try to improve compatibility as time permits.
Not every game runs with this emulator - but 90% will given a little elbow-grease to configure things right.

The emulator supports multi-disk games. When you need to load a subsequent disk for
a game, just use the Y button to disable Boot-Load which will simply insert the new
disk and you can continue to run. Not all games will utilize a 2nd disk drive but D2: is 
available for those games that do. It's handy to have a few blank 90K single-sided disks 
available on your setup which you can find easily online - these can be used as save disks.

The .ATR disk support handles up to 360K disks (it will probably work with larger disks, but has not 
been extensively tested beyond 360K). Generally you should stick to more standard disk sizes on 
this emulator - these are either 90K, 130K and sometimes 180K. Whenever possible, use a blank disk
to save your game progress - by default, disks mount in the drives as READ-ONLY and you must 
toggle this (via the X button on loading) if you want to write to a disk. Use caution!

The emulator has the built-in Altirra BASIC 1.58 which is a drop-in replacement for the
Atari Basic Rev C. Normally you can leave this disabled but a few games do require the 
BASIC cart to be present and you can toggle this with the START button when you load
a game. Be aware that the Altirra BASIC is faster than normal ATARI BASIC and so games 
might run at the wrong speed unless you're using the actual ATARI REV C rom.

A8DS emulates a Real-Time clock mostly used for SpartaDOS X.

Cartridge support was added with A8DS 3.1 and later. You can load .CAR and .ROM 
files (using the XEX/CAR button on the main screen).

The following cartridge layouts are supported:
* Standard 2K, 4K, 8K and 16K
* OSS two chip 16KB, OSS 8K
* Williams 16K, 32K and 64K
* Blizzard 4K, 16K and 32K
* XEGS/SwXEGS 32K up to 1MB
* MegaCart 16K up to 1MB
* Atarimax 128K and 1MB
* SpartaDOS X 64K and 128K
* Atrax 128K
* Diamond 64K
* Express 64K
* AST 32K
* Ultracart 32K
* Lowbank 8K
* Bounty Bob Strikes Back 40K
* SIC 128K, 256K and 512K
* Turbosoft 128K and 256K
* J(Atari) 8K to 1024K
* aDawliah 32K and 64K
* JRC64
* DCART
* Corina 1MB+EE and 512K + SRAM + EE
* MIO 8K
* Right-side 4K and 8K
* Atari 5200 Carts from 4K up to 32K (rename your 5200 carts to .a52 for easy loading)

If you're using cartridge files, it is suggested you use .CAR files which contain type information to properly load up the cartirdge. Bare .ROM files 
have ambiguities that are not always auto-detected by the emulator and as such will not always load correctly. You can go into the GEAR/Options menu
to force a cartridge type and save it for that game.

Missing :
----------------------------------------------------------------------------------
The .ATX support is included but not fully tested so compatibility may be lower. In order to 
get proper speed on the older DS-LITE and DS-PHAT hardware, there is a Frame Skip option that 
defaults to ON for the older hardware (and OFF for the DSi or above). This is not perfect - some 
games will not be happy to have frames skipped as collisions are skipped in those frames. 
Notably: Caverns of Mars, Jumpman and Buried Bucks will not run right with Frame Skip ON. But 
this does render most games playable on older hardware. If a game is particularly struggling 
to keep up on older hardware, there is an experimental 'Aggressive' frameskip which should help... but use with caution. 

Remember, emulation is rarely perfect. Further, this is a portable implementation of an Atari 8-bit 
emulator on a limited resource system (67MHz CPU and 4MB of memory) so it won't match the amazing output 
of something like Altirra. If you're looking for the best in emulation accuracy - Altirra is going to 
be what you're after. But if you want to enjoy a wide variety of classic 8-bit gaming on your DS/DSi 
handheld - A8DS will work nicely.

Known Issues :
----------------------------------------------------------------------------------
* On the non-A8DSi build, the Antic and GTIA accuracy is lower (but emulation is faster) and this will cause graphical glitches on some complex games (Atari Blast, Jim Slide XL, Bubbleshooter, etc.)
* Gun Fright requires that you press and hold the '3' button to start the game multiple times. Unknown cause but modern Atari800 emulator seems the same here.
* Intellidiscs (no discs sounds in game) - cause unknown.
* Rewind demo seems to be missing a sound channel. Cause unknown.

Troubleshooting :
----------------------------------------------------------------------------------
Most games run as-is. Pick game, load game, play game, enjoy game.

If a game crashes (crash message shows at bottom of screen or game does not otherwise run properly), check these in the order they are shown:

1. Try turning BASIC ON - some games (even a handful of well-known commercial games) require the BASIC cartridge be enabled. 
   If the game runs but is too fast with BASIC on, use the Atari Rev C Basic (slower but should run at proper speed).
2. If BASIC ON didn't do the trick, turn it back off and switch from the ALTIRRA OS to the real ATARI XL OS (you will need 
   atarixl.rom in the same directory as the emulator). Some games don't play nice unless you have the original Atari BIOS.
3. Next try switching from NTSC to PAL or vice-versa and restart the game.
4. A few older games require the older Atari 800 48k machine and Atari OS-B. If you have atariosb.rom where your emulator is located, 
   you can try selecting this as the OS of choice.
5. Lastly, try switching the DISKS SPEEDUP option to OFF to slow down I/O. Some games check this as a form of basic copy-protection 
   to ensure you're running from a legit disk.

With those tips, you should be able to get most games running. There are still a few odd games will not run with the emulator - such is life!

Installation :
----------------------------------------------------------------------------------
* To run this on your DS or DSi (or 2DS/3DS) requires that you have the ability to launch homebrews. For the older DS units, this is usually accomplished via a FlashCart such as the R4 or one of the many clones. These tend to run about US$25. If you have a DSi or above, you can skip the R4 and instead soft-mod your unit and run something like Twilight Menu++ or Unlaunch which will run homebrew software on the DS. The DSi has a convenient SD card slot on the side that saw very little use back in the day but is a great way to enjoy homebrews. See https://dsi.cfw.guide/ to get started on how to soft-mod your unit.
* You will want the optional BIOS Files for maximum compatibility. See BIOS section above.
* You will also need the emulator itself. You can get this from the GitHub page - the only file you need here is A8DS.nds (the .nds is a executable file). You can put this anywhere - most people put the .nds file into the root of the SD card.
* You will need games or applications in .XEX .ATR .ATX .CAR or .ROM format. Don't ask me for such files - you will be ignored.

DS vs DSi vs DSi XL/LL :
----------------------------------------------------------------------------------
The original DS-Lite or DS-Phat require an R4 card to run homebrews. With this setup you will be running in DS compatibility mode and emulator will default to a moderate level of frameskip. For the DSi or DSi XL/LL we can run just about everything full speed without frameskip. The XL/LL has a slightly slower decay on the LCD and it more closely mimics the phosphor fade of a TV. This helps with games that use small bullets - something like Centipede can be a little harder to see on the original DSi as the thin pixel shot fades quickly as it moves. You can somewhat compensate for this by increasing your screen brightness. For the DSi, I find a screen brightness of 4 to offer reasonably good visibility. The XL/LL will generally operate just as well with a brightness of 2 or 3. 

Screen resolution on a DS/DSi/XL/LL is always fixed at 256x192 pixels. The Atari 8-bit resolution tends to be larger - usually 320 horizontally and they often utilize a few more pixels vertically depending on the game. Use the Left/Right shoulder buttons along with the D-Pad to shift/scale the screen to your liking. Once you get the screen where you want - go into the GEAR icon and press START to save the options (including the screen position/scaling) on a per-game basis.

Configuration :
----------------------------------------------------------------------------------
Global Options can now be set - *before* you load a game, use the GEAR icon to set and save your options that will be applied to new games going forward (you can still override on a per-game basis).

* MACHINE TYPE - There are seven possible machine configurations ranging from the Atari 5200 (16K of RAM) and Atari 800 (48K of RAM) all the way up to the XL/XE with 1088K of RAM.  The default machine is the standard XL/XE with 128K of RAM.
* TV TYPE - Select PAL vs NTSC for 50/60Hz operation. The proper color palette will be swapped in automatically.
* BASIC - Select if BASIC is Enabled or Disabled.
* CART TYPE - If you load a Cartridge via a .CAR file, it should automatically pick the right Cart type. If you load via a .ROM file it will take a guess but it might not be right - so you can override (and SAVE) it here.
* SKIP FRAMES - On the DSi you can keep this OFF for most games, but for the DS you may need a moderate-to-agressive frameskip.
* FPS SETTING - Normally OFF but you might want to see the frames-per-second counter and you can set 'TURBO' mode to run full-speed (unthrottled) to check performance.
* ARTIFACTING - Normally OFF but a few games utilize this high-rez mode trick that brings in a new set of colors to the output.
* SCREEN BLUR - Since the DS screen is 256x192 and the Atari A8 output is 320x192 (and often more than 192 pixels utilizing overscan area), the blur will help show fractional pixels. Set to the value that looks most pleasing (and it will likely be a different value for different games). Usually LIGHT is okay for most games. Be aware that the DSi XL has some LCD memory effect (only when power is applied... so it's not long-term) where blur might leave some visual artifacts on screen as a sort of short-term burn-in.
* ALPHA BLEND - The DSi non-XL handhelds tend to have a fast LCD fade and that can make it hard to see small objects. Turn this ON to blend two successive frames. This has the effect of making the screen a bit lighter/brighter and small details tend to show more clearly.
* DISK SPEEDUP - the SIO access is normally sped-up but a few games on disk (ATR/ATX) won't run properly with disk-speedup so you can disable on a per-game basis.
* KEY CLICK - if you want the mechanical key-click when using the virtual 800 keyboards.
* EMULATOR TEXT - if you want a clean main screen with just the disk-drives shown, you can disable text.
* KEYBOARD STYLE - select the style of virtual keyboard that you prefer.

Using the X button, you can go to a second menu of options mostly for key handling.  This menu allows you to map any DS key to any of the A8DS functions (joystick, keyboard, console switches and a few 'meta' commands such as smooth scrolling the screen some number of pixels).

Screen Scaling and Smooth Scrolling :
----------------------------------------------------------------------------------
An NTSC Atari 800 uses a video chip that outputs 320 x 192 (nominal). Many games utilize the overscan and underscan areas. Further, PAL systems utilize more scanlines. This is unfortunate for our hero the DS/DSi which has a fixed resolution of 256x192. As such, the system must scale the video image down - losing pixel rows and columns as it does so. A8DS allows for some help in this department - you can use the Gear/Settings to tweak the scaling and offsets to get as many usable pixels onto the screen (for example, some games may utilize a "sky" or "ground" area that isn't critical for gameplay and can safely be off-screen).

One other trick is the use of Smooth Scroll. You can assign to any DS key (I usually use the X and Y keys) one of the following 'meta' functions:

* VERTICAL+ or VERTICAL- ... shift the screen up or down 16 pixels and smooth-scroll it back into place
* VERTICAL++ or VERTICAL-- ... shift the screen up or down 32 pixels and smooth-scroll it back into place
* HORIZONTAL+ or HORIZONTAL- ... shift the screen left or right 32 pixels and smooth-scroll it back into place
* HORIZONTAL++ or HORIZONTAL-- ... shift the screen left or right 64 pixels and smooth-scroll it back into place

This really helps games where there might be some score other infrequently referenced information at the top/bottom of the screen. Witness here Caverns of Mars which I've mapped to show the entire screen minus the bottom row of text... using VERTICAL+ I can press the X button to quickly glance at the bottom line and let it scroll back up during gameplay. 

![Caverns of Mars](https://github.com/wavemotion-dave/A8DS/blob/main/png/caverns_02.bmp)
![Caverns of Mars](https://github.com/wavemotion-dave/A8DS/blob/main/png/caverns_01.bmp)

Lastly - if a game was released for both NTSC and PAL and does not utilize the same number of visible scanlines, you're usually better off with the NTSC versions on the DS handheld. The reason is simple: you are less likely to have to scale and drop pixel rows with an NTSC output.  However, with the use of scaling and smooth scrolling, you should be able to get workable versions of games in either TV standard.

Default Controller Mapping :
----------------------------------------------------------------------------------
Buttons can be re-configured in the Options (GEAR icon... press X for the keyboard map area)

 * D-pad  : the joystick ... can be set to be Joystick 1 or Joystick 2
 * A      : Fire button
 * B      : Alternate Fire button
 * X      : Vertical pan up
 * Y      : Return key (Carriage Return)
 * R+Dpad : Shift Screen UP or DOWN (necessary to center screen)
 * L+Dpad : Scale Screen UP or DOWN (generally try not to shrink the screen too much as pixel rows disappear)
 * L+R    : Hold for 1 second to snapshot upper screen to .bmp file on the SD card
 * L+R+A  : Swap Screens (swap the upper and lower screens... touch screen is still always the bottom)
 * START  : START console button
 * SELECT : SELECT console button
 
Tap the XEX icon or the Disk Drive to load a new game or the Door/Exit button to quit the emulator.

Saving and Restoring Emulator State :
-----------------------
A8DS has the ability to save memory/cpu and restore it so you can snap out a save state and restore it back. Use the icons on the main screen.
The DOWN arrow icon saves state (with confirmation).
The UP arrow icon restores state (with confirmation).
The .sav files have the same name as the game/rom you are playing and are stored in the 'sav' folder under where you keep your game roms. 
Note: the emulator does not save startup information so you should let any game come up to the natural title screen before trying to restore state.

Compile Instructions :
-----------------------
I'm using the following:
* devkitpro-pacman version 6.0.1-2
* gcc (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0
* libnds 1.8.2-1

I use Ubuntu and the Pacman repositories (devkitpro-pacman version 6.0.1-2).  I'm told it should also build under 
Windows but I've never done it and don't know how.

If you try to build on a newer gcc, you will likely find it bloats the code a bit and you'll run out of ITCM_CODE memory.
If this happens, first try pulling some of the ITCM_CODE declarations in and around the Antic.c module (try to leave
the one in CPU as it has a big impact on performance).  

--------------------------------------------------------------------------------
History :
--------------------------------------------------------------------------------
V4.2  : ??-Feb-2025 by wavemotion-dave
  * Fixed Turbo 128K cart type so it doesn't inadvertently disable the cartridge port.
  * Added Right-Side cart support for Atari800 (fixing a few A800 emulation issues as well).
  * Added Atari 5200 cart support for carts of 32K or less (rename your 5200 carts to ".a52" for easy loading)
  * Added new key maps for SHIFT and CONTROL to NDS keys.
  * Massive simplification of the configuration handling for machine type.
  * New NTSC and PAL color palettes from the awesome Trebor Pro Pack.
  * Added new cart types for SIC+ (1MB), Corina (1MB+EE and 512K+512K+EE), Telelink II and MIO_8

V4.1  : 25-Jan-2025 by wavemotion-dave
  * Refactor the OS Enable/Disable on XL/XE emulation to avoid moving large blocks of memory. Speeds up many games that swap the OS in/out.
  * Improved SIO/Disk handling - sound effects reduced in volume and now configurable (default is OFF).
  * Improved cart/disk loading and all disks are left mounted when RESET is pressed allowing you to load up a system the way you want.
  * A dozen new .CAR cartridge types supported including DCART (Bubble Bobble homebrew).
  * Tweak to the TWL++ icon for A8DS to make it look a bit more classic.
  * Many small cleanups and improvements and a few bug fixes as well.

V4.0  : 20-Jan-2025 by wavemotion-dave
  * Major overhaul to add "Cycle Exact" Antic and GTIA which fixes many glitches and artifacts.
  * There are now two builds... one for the older DS-Phat/Lite and one for the DSi (or XL/LL) which brings in a higher level of compatibility with the "Cycle Exact" timing.
  * Improved keyboard overlays - added alphanumeric keyboard with text-adventure macros.
  * Improved memory handling - using more memory but in an efficient way for the new features.
  * Improved sound handling - new SIO sounds, new opening jingle.
  * Improved CPU handling to fix one more Acid800 test (25 pass now).
  * Fixed keyboard handling so games like Scorch will register keypresses (broken as of V3.0).
  * Added some of the more obscure missing .CAR cartridge types.  
  * So much changed under the hood that old config files will be wiped and old save states will not work. Sorry!

V3.9  : 13-Jan-2025 by wavemotion-dave
  * Altirra OS updated to 3.41 (Altirra BASIC still at 1.58)
  * Touch-up on the keyboard graphics to make the smaller font bolder / more readable on DS screen
  * Fixed on Acid800 test - we now comply with the JMP indirect bug on the 6502.
  * PIA emulation improvements to match latest Atari800 core.

V3.8a : 12-Jan-2024 by wavemotion-dave
  * Optmization of the sound core to help reduce scratchy sounds.

V3.8 : 03-Jan-2024 by wavemotion-dave
  * Optmization of CPU core for a 3% speedup across the board.
  * New Star Raiders keypad overlay integrated into the emulator.
  * Minor tweaks, fixes and cleanup as time permitted.
  
V3.7a : 17-June-2023 by wavemotion-dave
  * Improved CRC32 of ATR files so that disks that write new content (high scores, save states, etc.) will still bring back settings properly.

V3.7 : 04-June-2023 by wavemotion-dave
  * Update to Screen Blur to have just 3 settings: NONE, LIGHT and HEAVY. Default is LIGHT.
  * Improvements to memory layout to gain back additional resources.
  * Fix for 576K COMPY RAM so that it properly handles separate ANTIC memory access.
  * Minor fixes and cleanup as time permitted.

V3.6a : 30-May-2023 by wavemotion-dave
  * Hotfix for state save/restore. Sorry!
  * Added ability to map Joystick 2 so you can play twin-stick games like Robotron and Space Dungeon.
  * Added ability to map the Atari HELP key.
  * Added 64K memory option and put all memory options in the correct order.

V3.6 : 29-May-2023 by wavemotion-dave
  * Added the ability to save and restore state - use the DOWN/UP icons on the main screen.
  * Minor memory optimization to squeeze out another frame or two of performance.

V3.5 : 22-May-2023 by wavemotion-dave
  * Added 576K COMPY SHOP RAM type (with separate ANTIC access just like the 128K XE).
  * More cleanup and minor bug fixes across the board.

V3.4 : 16-May-2023 by wavemotion-dave
  * Default to using ATARI OS if bios files found.
  * Altirra OS updated to 3.33 and Altirra BASIC to 1.58
  * High Score saving added - save 10 scores per game.
  * Improved PAL vs NTSC color palette
  * Several config bugs that necessitated another quick release. Sorry!

V3.3 : 15-May-2023 by wavemotion-dave
  * Switched to CRC32 (from md5sum) to save space and now allow 2500 game settings to be stored.
  * Added additional cartridge banking schemes so more games run.
  * Added ability to change/save a cartridge type in settings.
  * Tweak of VERTICAL+ and VERTICAL- to offset by 16 pixels (was 10).
  * Reduced memory footprint to allow for better future expansion.

V3.2 : 13-May-2023 by wavemotion-dave
  * Enhanced configuration - unfortunately your old config save will be wiped to make way for the new method.
  * Global options - use the GEAR icon before a game is loaded and you can save out defaults for newly loaded games.
  * Key maps - set any of the DS keys to map into various joystick, console buttons, keyboard keys, etc. 
  * Screenshot capability - press and hold L+R for ~1 second to take a .bmp snapshot (saved to a time-date.bmp file)
  * New Smooth Scroll handling so you can set your scale/offset and then map any button to shift vertical/horizontal pixels (set keys to VERTICAL++, HORIZONTAL--, etc). The game will automatically smooth-scroll back into place when you let go of the pixel-shift button.
  * Improved cart banking so that it's as fast as normal memory swaps. This should eliminate slowdown in Cart-based games.
  * A few bug fixes as time permitted.

V3.1 : 08-May-2023 by wavemotion-dave
  * Added CAR and ROM support for the more popular cartridge types up to 1MB.
  * Added Real-Time Clock support for things like SpartaDOS X
  * Added new D-Pad options to support joystick 2 (for games like Wizard of Wor) and diagonals (Q-Bert like games).
  * Improved keyboard handling so CTRL key is now sticky.
  * Improved menu transitions to reduce audio 'pops' as much as possible.
  * Auto-rename of XEGS-DS.DAT to A8DS.DAT to match new branding.
  * Squeezed as much into fast ITCM_CODE as possible with almost no bytes left to spare.
  * Other cleanups and minor bug fixes as time allowed.

V3.0 : 05-May-2023 by wavemotion-dave
  * Rebranding to A8DS with new 800XL stylized keyboard and minor cleanups across the board.
  
V2.9 : 12-Dec-2021 by wavemotion-dave
  * Reverted back to ARM7 SoundLib (a few games missing key sounds)
  
V2.8 : 30-Nov-2021 by wavemotion-dave
  * Switched to maxmod audio library for improved sound.
  * Try to start in /roms or /roms/a800 if possible

V2.7 : 04-Nov-2021 by wavemotion-dave
  * New sound output processing to eliminate Zingers!
  * bios files can now optionally be in /roms/bios or /data/bios
  * Left/Right now selects the next/previous option (rather than A button to only cycle forward).
  * Other cleanups as time permitted.

V2.6 : 11-Jul-2021 by wavemotion-dave
  * Reduced down to one screen buffer - this cleans up ghosting visible sometimes on dark backgrounds.
  * If atarixl.rom exists, it is used by default (previously had still been defaulting to Altirra rom)
  * Minor cleanups as time permitted.

V2.5 : 08-Apr-2021 by wavemotion-dave
  * Major cleanup of unused code to get down to a small but efficient code base.
  * Added LCD swap using L+R+A (hold for half second to toggle screens)
  * Cleanup of text-on-screen handling and other minor bug fixes.

V2.4 : 02-Apr-2021 by wavemotion-dave
  * New bank switching handling that is much faster (in some cases 10x faster)
    to support all of the larger 128K, 320K and even the 1088K games (AtariBlast!)
  * ATX format now supported for copy protected disk images.

V2.3 : 31-Mar-2021 by wavemotion-dave
  * Added Atari 800 (48K) mode with OS-B for compatiblity with older games.
  * L+X and R+X shortcuts for keys '1' and '2' which are useful to start some games.
  * Cleanup of options and main screen for better display of current emulator settings.

V2.2 : 25-Mar-2021 by wavemotion-dave
  * Added simplified keyboard option for easy use on Text Adventures, etc.

V2.1 : 21-Mar-2021 by wavemotion-dave
  * Cleanup of the big 2.0 release... 
  * Allow .XEX and D1 to both be loaded for XEX games that allow save/restore.
  * Fixed long-standing file select offset bug.

V2.0 : 19-Mar-2021 by wavemotion-dave
  * Major overhaul of UI.
  * Added second disk drive.

V1.9 : 06-Mar-2021 by wavemotion-dave
  * New options for B button = DOWN and Key Click Off.
  * Improved handling for key clicks so that press and hold will auto-repeat.

V1.8 : 25-Feb-2021 by wavemotion-dave
  * Added option for slower I/O (disk reads) as a few games will detect that
    the game is not running at the right speed and not play (copy protection 
    of a sort ... 1980s style). So you can now slow down the I/O to get those
    games running.
  * Reverted to "Old NTSC Artifacting" after discovering at least one game
    does not play nicely with the new artificating. Still investigating but
    this cures the problem for now (the game was Stellar Shuttle).
  * Added the R-TIME8 module for time/date on some versions of DOS.
  * Other minor cleanups as time permitted.

V1.7 : 18-Feb-2021 by wavemotion-dave
  * Added saving of configuration for 1800+ games. Press L+R to snap out config
    for any game (or use the START key handling in the Options Menu).
  * Autofire now has 4 options (OFF, Slow, Medium and Fast).
  * Improved pallete handling.
  * Improved sound quality slightly.
  * Other cleanups as time permitted.

V1.6 : 13-Feb-2021 by wavemotion-dave
  * Added 320KB RAMBO memory expanstion emulation for the really big games!
  * Added Artifacting modes that are used by some high-res games.
  * Improved option selection - added brief help description to each.
  * Improved video rendering to display high-res graphics cleaner.
  * Fixed directory/file selection so it can handle directories > 29 length.

V1.5 : 10-Feb-2021 by wavemotion-dave
  * Added Frame Skip option - enabled by default on DS-LITE/PHAT to gain speed.
  * Minor cleanups and code refactor.

V1.4 : 08-Feb-2021 by wavemotion-dave
  * Fixed Crash on DS-LITE and DS-PHAT
  * Improved speed of file listing
  * Added option for X button to be SPACE or RETURN 

V1.3 : 08-Feb-2021 by wavemotion-dave
  * Fixed ICON
  * Major overhaul to bring the SIO and Disk Loading up to Atari800 4.2 standards.
  * New options menu with a variety of options accessed via the GEAR icon.

V1.2 : 06-Feb-2021 by wavemotion-dave
  * Altirra BASIC 1.55 is now baked into the software (thanks to Avery Lee).
  * Fixed PAL sound and framerate.
  * Cleanups and optmizations as time permitted.
  * Tap lower right corner of touch screen to toggle A=UP (to allow the A key
    to work like the UP direction which is useful for games that have a lot
    of jumping via UP (+Left or +Right). It makes playing Alley Cat and similar
    games much more enjoyable on a D-Pad.

V1.1 : 03-Feb-2021 by wavemotion-dave
  * Fixed BRK on keyboard.
  * If game crashes, message is shown and emulator no longer auto-quits.
  * Improved loading of games so first load doesn't fail.
  * Drive activity light, file read activity for large directories, etc.
  
V1.0 : 02-Feb-2021 by wavemotion-dave
  * Improved keyboard support. 
  * PAL/NTSC toggle (in game select). 
  * Ability to load multiple disks for multi-disk games (in game select).
  * Other cleanups as time permits...

V0.9 : 31-Jan-2021 by wavemotion-dave
  * Added keyboard support. Cleaned up on-screen graphics. Added button support.

V0.8 : 30-Jan-2021 by wavemotion-dave
  * Alpha release with support for .XEX and .ATR and generally runs well enough.
 




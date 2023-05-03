A8DS
--------------------------------------------------------------------------------
A8DS is an Atari 8-bit computer emulator.  Specifically, it targets the 
800XL / 130XE systems and various hardware extensions to increase the memory. 
The stock 800XL had 64KB of RAM. The default A8DS configuration is an
XL/XE machine with 128KB of RAM which will run most of the 8-bit library.
A8DS goes beyond the normal XL/XE 128K machine and provides three alternative 
configurations: the 320K (RAMBO) and 1088K for a few large games and demos 
plus an Atari 800 (non-XL) 48K machine for backwards compatibility with some older 
games that don't play nice with a more "modern" XL/XE setup. As such, it's 
really grown to be a full-featured 8-bit emulator to run nearly the entire 
8-bit line up of games on their Nitendo DS/DSi handhelds.

The emulator comes "equipped" with the ability to run executable images or disk 
images which are the two most popular types. It does not support cart types yet.
Virtually everything that can be run on an 8-bit system has been converted 
into an Executable image (.xex) or a Disk image (.atr) and, unlike cart
ROM images, there is no complicated setup to pick the right banking setup. 
The goal here is to make this as simple as possible - point to the executable
8-bit Atari image you want to run and off it goes!

![A8DS](https://github.com/wavemotion-dave/A8DS/blob/main/arm9/gfx/bgTop.png)


Optional BIOS Roms
----------------------------------------------------------------------------------
There is a built-in Altirra BIOS (thanks to Avery Lee) which is reasonably compatibile
with many games. However, a few games will require the original ATARI BIOS - and,
unfortunately, there were many variations of those BIOS over the years to support
various Atari computer models releaesed over a decade.

A8DS supports 3 optional Atari BIOS and BASIC files as follows:

*  atarixl.rom   - this is the 16k XL/XE version of the Atari BIOS for XL/XE Machines
*  atariosb.rom  - this is the 12k Atari 800 OS-B revision BIOS for older games
*  ataribas.rom  - this is the 8k Atari BASIC cartridge (recommend Rev C)

You can install zero, one or more of these files and if you want to use these real ROMs
they must reside in the same folder as the A8DS.NDS emulator or you can place your
BIOS files in /roms/bios or /data/bios) and these files must be exactly
so named as shown above. These files are loaded into memory when the emulator starts 
and remain available for the entire emulation session. Again, if you don't have a real BIOS, 
a generic but excellent one is provided from the good folks who made Altirra (Avery Lee) 
which is released as open-source software.  Also optional is ataribas.rom for the 8K basic 
program (Rev C is recommended). If not supplied, the built-in Altirra BASIC 1.55 is supplied.

I've not done exhaustive testing, but in many cases I find the Altirra BIOS does a
great job compared to the original Atari BIOS. I generally stick with the open source
Altirra BIOS if it works but you can switch it on a per-game basis in the Options menu.

Do not ask me about rom files, you will be propmptly ignored. A search with Google will certainly 
help you. 

Copyright:
--------------------------------------------------------------------------------
A8DS - Atari 8-bit Emulator designed to run 8-bit games on the Nitendo DS/DSi
Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)

Copying and distribution of this emulator, it's source code and associated 
readme files, with or without modification, are permitted in any medium without 
royalty provided the this copyright notice is used and wavemotion-dave, alekmaul 
(original port) and Avery Lee (Altirra OS) are credited and thanked profusely.

The A8DS emulator is offered as-is, without any warranty.


Credits:
--------------------------------------------------------------------------------
* Thanks Wintermute for devkitpro and libnds (http://www.devkitpro.org).
* Atari800 team for source code (http://atari800.sourceforge.net/)
* Altirra and Avery Lee for a kick-ass substitute BIOS and generally being awesome.
* Alekmaul for porting the original A5200DS of which this is heavily based.
* Darryl Hirschler for the awesome Atari 8-bit Keyboard Graphic.
* The good folks over on GBATemp and AtariAge for their support.


Features :
----------------------------------------------------------------------------------
Most things you should expect from an emulator. Games generally run full-speed
with just a handful of exceptions. If you load a game and it doesn't load properly,
just load it again or hit the RESET button which will re-initialize the A8DS machine.
If a game crashes, you will get a message at the bottom of the screen after loading - 
a crash usually means that the game requires the BASIC cart to be inserted and you can
toggle that when loading a game (or using the GEAR icon). Not every game runs with this 
emulator - but 99% will.  I'll try to improve compatibilty as time permits.

The emulator supports multi-disk games. When you need to load a subsequent disk for
a game, just use the Y button to disable Boot-Load which will simply just insert the new
disk and you can continue to run. Not all games will utilize a 2nd disk drive but D2: is 
available for those games that do. It's handy to have a few blank 90K single-sided disks 
available on your setup which you can find easily online - these can be used as save disks.

The .ATR disk support handles up to 360K disks (it may work with larger disks, but has not 
been extensively tested beyond 360K). 

The emulator has the built-in Altirra BASIC 1.55 which is a drop-in replacement for the
Atari Basic Rev C (only more full-featured). Normally you can leave this disabled but
a few games require the BASIC cart be present and you can toggle this with the START button
when you load a game.  If you try to load a game and it crashes, most likely you need
to have BASIC enabled. Most games don't want it enabled so that's the default. 
If you want to play around with BASIC, enable the BASIC cart and pick a DOS 
disk of some kind to get drive support and you can have fun writing programs. Be aware
that the Altirra BASIC is faster than normal ATARI BASIC and so games might run at the
wrong speed unless you're using the actual ATARI REV C rom.

Missing :
----------------------------------------------------------------------------------
Not much.  In order to get proper speed on the older DS-LITE and DS-PHAT hardware, there
is a Frame Skip option that defaults to ON for the older hardware (and OFF for the DSi 
or above). This is not perfect - some games will not be happy to have frames skipped as
collisions are skipped in those frames. Notably: Caverns of Mars, Jumpman and Buried Bucks will 
not run right with Frame Skip ON. But this does render most games playable on older hardware.
If a game is particularlly struggling to keep up on older hardawre, there is an experimental
'Agressive' frameskip which should help... but use with caution. 

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

With those tips, you should be able to get 99% of all games running. There are still a few odd "never heard of" games of little 
or no consequence (i.e. not major titles) that will not run with the emulator - such is life!

Controls :
----------------------------------------------------------------------------------
 * D-pad  : the joystick ...
 * A      : Fire button
 * B      : Alternate Fire button
 * X      : Space Bar (R+X for RETURN and L+X for ESCAPE) - useful to start a few games
 * R+Dpad : Shift Screen UP or DOWN (necessary to center screen)
 * L+Dpad : Scale Screen UP or DOWN (generally try not to shrink the screen too much as pixel rows disappear)
 * L+R+A  : Swap Screens (swap the upper and lower screens... touch screen is still always the bottom)
 * Y      : OPTION console button
 * START  : START console button
 * SELECT : SELECT console button
 
Tap the XEX icon or the Disk Drive to load a new game.

--------------------------------------------------------------------------------
History :
--------------------------------------------------------------------------------
V3.0 : xx-May-2023 by wavemotion-dave ... TBD coming soon
  * Rebranding to A8DS with new 800XL stylized keyboard.
  
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
 




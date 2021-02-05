XEGS-DS
--------------------------------------------------------------------------------
XEGS-DS is an Atari 8-bit computer emulator.  Specifically, it's designed 
around the XEGS gaming system which is the 8-bit console version of Atari's
venerable computer line. The stock XEGS had 64KB of RAM and was generally
not upgradable unless you really knew the insides of the machine... but thanks
to emulation, the XEGS-DS comes equipped with 128KB of RAM which will run the 
larger programs. This is, essentially, the same as a 130XE machine.

The emulator comes "equipped" with the ability to run executable images or disk 
images which are the two most popular types. It does not support cart types
(despite the cart slot as the on-screen mechanism for choosing a game!). 
Virtually everything that can be run on an 8-bit system has been converted 
into an Executable image (.xex) or a Disk image (.atr) and, unlike cart
ROM images, there is no complicated setup to pick the right banking setup. 
The goal here is to make this as simple as possible - point to the executable
8-bit Atari image you want to run and off it goes!

To use this emulator, you must use compatibles roms with .xex or .atr format. 
The .xex is preferred as that is a stand-alone binary executable for the 
Atari 8-bit line and will almost always load correctly. The .atr is a disk 
drive encapsulated format and is a little tricker and compatibility will not
be as high. You can often find binary files in both formats - so stry to find
the .xex if you can.

Optional is to have the atarixl.rom in the same folder as the XEGS-DS.NDS emulator. 
This is the 16k bios file and must be exactly so named. If you don't have the BIOS, 
a generic one is provided from the good folks who made Altirra (thanks to Avery Lee)
have released open-source. 

I've not done exhaustive testing, but in many cases I find the Altirra BIOS does 
a better job than the original Atari BIOS. I generally stick with the open source
Altirra BIOS. 

Do not ask me about rom files, I don't have them. A search with Google will certainly 
help you. 

Features :
----------
Most things you should expect from an emulator. Games generally run full-speed
with just a handful of exceptions. If you load a game and it doesn't load properly,
just load it again or hit the RESET button which will re-initialize the XEGS machine.
If a game crashes, you will get a message at the bottom of the screen after loading.
Not every game runs with this emulator - but most do.  I'll try to improve compatibilty
as time permits.

The emulator does support multi-disk games. When you need to load a subsequent disk for
a game, just use the Y button to disable Boot-Load which will simply just insert the new
disk and you can continue to run. 

The emulator has the built-in Altirra BASIC 1.55 which is a drop-in replacement for the
Atari Basic Rev C (only more full-featured). Normally you can leave this disabled but
a few games require the BASIC cart be present and you can toggle this with the START button
when you load a game. If you want to play around with BASIC, enable the BASIC cart and
pick a DOS II disk of some kind to get drive support and you can have fun writing programs.

Missing :
---------
Not much.

--------------------------------------------------------------------------------
History :
--------------------------------------------------------------------------------
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

Controls :
 * D-pad  : the joystick ...
 * A      : Fire button
 * B      : Alternate Fire button
 * X      : Space Bar (R+X for RETURN and L+X for ESCAPE) - useful to start a few games
 * R+Dpad : Shift Screen UP or DOWN (necessary to center screen)
 * L+Dpad : Scale Screen UP or DOWN (generally try not to shrink the screen too much as pixel rows disappear)
 * Y      : OPTION console button
 * START  : START console button
 * SELECT : SELECT console button
 
Tap in the Cart Slot to pick a new game.
 
--------------------------------------------------------------------------------
Credits:
--------------------------------------------------------------------------------
Thanks Wintermute for devkitpro and libnds (http://www.devkitpro.org).
Atari800 team for source code (http://atari800.sourceforge.net/)
Altirra and Avery Lee for a kick-ass substitute BIOS and generally being awesome.
Alekmaul for porting the A5200DS of which this is heavily based.
--------------------------------------------------------------------------------


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

Optional is to have the atarixl.rom in the same folder as the XEGS-DS.NDS emulator. 
This is the 16k bios file and must be exactly so named. If you don't have the BIOS, 
a generic one is provided from the good folks who made Altirra (thanks to Avery Lee)
have released open-source.  Also optional is ataribas.rom for the 8K basic program 
(Rev C is recommended). If not supplied, the built-in Altirra BASIC 1.55 is supplied.

I've not done exhaustive testing, but in many cases I find the Altirra BIOS does 
a great job compared to the original Atari BIOS. I generally stick with the open 
source Altirra BIOS but you can switch it on a per-game basis in the Options menu.

Do not ask me about rom files, I don't have them. A search with Google will certainly 
help you. 

Features :
----------
Most things you should expect from an emulator. Games generally run full-speed
with just a handful of exceptions. If you load a game and it doesn't load properly,
just load it again or hit the RESET button which will re-initialize the XEGS machine.
If a game crashes, you will get a message at the bottom of the screen after loading - 
a crash usually means that the game requires the BASIC cart to be inserted and you can
toggle that when loading a game (or using the GEAR icon). Not every game runs with this 
emulator - but 99% will.  I'll try to improve compatibilty as time permits.

The emulator supports multi-disk games. When you need to load a subsequent disk for
a game, just use the Y button to disable Boot-Load which will simply just insert the new
disk and you can continue to run. 

The emulator has the built-in Altirra BASIC 1.55 which is a drop-in replacement for the
Atari Basic Rev C (only more full-featured). Normally you can leave this disabled but
a few games require the BASIC cart be present and you can toggle this with the START button
when you load a game.  If you try to load a game and it crashes, most likely you need
to have BASIC enabled. Most games don't want it enabled so that's the default. 
If you want to play around with BASIC, enable the BASIC cart and
pick a DOS II disk of some kind to get drive support and you can have fun writing programs.

Missing :
---------
Not much.  In order to get proper speed on the older DS-LITE and DS-PHAT hardware, there
is a Frame Skip option that defaults to ON for the older hardware (and OFF for the DSi 
or above). This is not perfect - some games will not be happy to have frames skipped as
collisions are skipped in those frames. Notably: Caverns of Mars and Buried Bucks will 
not run right with Frame Skip ON. But this does render most games playable on older hardware.

Troubleshooting :
------------------
Most games run as-is. Pick game, load game, play game, enjoy game.

If a game crashes (crash message shows at bottom of screen or game does not otherwise run properly), check these in the order they are shown:

1. Try turning BASIC ON - some games (even a handful of well-known commercial games) require the BASIC cartridge be enabled. If the game runs but is too fast with BASIC on, use the Atari Rev C Basic (slower but should run at proper speed).
2. If BASIC ON didn't do the trick, turn it back off and switch from the ALTIRRA OS to the real ATARI XL OS (you will need atarixl.rom in the same directory as the emulator). Some games don't play nice unless you have the original Atari BIOS.
3. Next try switching from NTSC to PAL or vice-versa and restart the game.\
4. Lastly, try switching the DISKS SPEEDUP option to OFF to slow down I/O. Some games check this as a form of basic copy-protection to ensure you're running from a legit disk.

With those 4 tips, you should be able to get 99% of all games running. There are still a few odd "never heard of" games of little or no consequence (i.e. not major titles) that will not run with the emulator - such is life!

--------------------------------------------------------------------------------
History :
--------------------------------------------------------------------------------
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


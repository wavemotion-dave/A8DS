XEGS-DS
--------------------------------------------------------------------------------
XEGS-DS is an Atari 8-bit computer emulator.  Specifically, it's designed 
around the XEGS gaming system which is the 8-bit console version of Atari's
venerable computer line. The stock XEGS had 64KB of RAM and was generally
not upgradable unless you really knew the insides of the machine... but thanks
to emulation, the XEGS-DS comes equipped with 128KB of RAM which will run the 
larger programs.

To use this emulator, you must use compatibles rom with .xex or .atr format. 
The .xex is preferred as that is a stand-alone binary executable for the 
Atari 8-bit line and will almost always load correctly. The .atr is a disk 
drive encapsulated format and is a little tricker and compatibility will not
be as high. You can often find binary files in both formats - so stry to find
the .xex if you can.

Optional is to have the atarixl.rom in the same folder as the XEGS-DS.NDS emulator. 
This is the 16k bios file and must be exactly so named.
If you don't have the BIOS, a generic one is provided from the good folks who
made Altirra have released open-source. This will get the job done 90% of the time
but is not as fully compatible as the original atarixl.rom bios.

I've not done exhaustive testing, but in some cases I find the Altirra BIOS does 
a better job than the original Atari BIOS - so you may want to switch them in
or out - one easy way to do this is to hold down either Right or Left Trigger 
when you launch the emulator which will force the Altirra bios to be used
even if the atarixl.rom bios is found.

Do not ask me about such files, I don't have them. A search with Google will certainly 
help you. 

Features :
----------
 Most things you should expect from an emulator.

Missing :
---------
 There is no multi-disk support. This is intended to be a simple gaming console
 to allow the nearly infinite number of 8-bit games to run - as such it's only
 going to load .XEX (Atari 8-bit executable files) and for convienence we also 
 allow .ATR (diskette format) files but we stop at loading just 1 diskette. If
 a game requires more than 1 diskette, this is not the emulator to use. 

--------------------------------------------------------------------------------
History :
--------------------------------------------------------------------------------
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
The Altirra for a kick-ass substitute BIOS
Alekmaul for porting the A5200DS of which this is heavily based.
--------------------------------------------------------------------------------


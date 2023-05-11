/*
 * MAIN.C contains the entry point for the ARM9 execution.
 * 
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)

 * Copying and distribution of this emulator, its source code and associated 
 * readme files, with or without modification, are permitted in any medium without 
 * royalty provided this full copyright notice (including the Atari800 one below) 
 * is used and wavemotion-dave, alekmaul (original port), Atari800 team (for the
 * original source) and Avery Lee (Altirra OS) are credited and thanked profusely.
 * 
 * The A8DS emulator is offered as-is, without any warranty.
 * 
 * Since much of the original codebase came from the Atari800 project, and since
 * that project is released under the GPL V2, this program and source must also
 * be distributed using that same licensing model. See COPYING for the full license.
 */
#include <stdio.h>
#include <fat.h>
#include <nds.h>
#include <unistd.h>

#include "main.h"
#include "atari.h"
#include "intro.h"
#include "a8ds.h"

extern void load_os(void);
extern void install_os(void);
extern int skip_frames;

// -----------------------------------------------------------------------
// Program entry point - for things like Twilight Menu++ (TWL++), we can 
// take a ROM (XEX or ATR) on the command line and we will auto-load that 
// and start the game running.
// -----------------------------------------------------------------------
int main(int argc, char **argv) 
{
    // ----------------------------------
    // Init basic sound and LCD use...
    // ----------------------------------
    consoleDemoInit();
    soundEnable();
    lcdMainOnTop();
    
    // --------------------------------------------------------------------
    // For older DS models, we force skip frames to get full speed...
    // This does come with some problems - skipping frames can cause
    // problems with a few games that don't get the collision detection
    // right with skipped frames - Caverns of Mars, Buried Bucks and 
    // Jumpman are examples of games that won't run right. The user can
    // adjust the frame skip in the Options (Gear icon) area.
    // --------------------------------------------------------------------
    skip_frames = (isDSiMode() ? FALSE:TRUE);

    // Init FAT for file system use...
    if (!fatInitDefault()) 
    {
        iprintf("Unable to initialize libfat!\n");
        return -1;
    }

    // Intro and main screen
    intro_logo();  
    dsInitScreenMain();
    emu_state = A8_MENUINIT;

    load_os();          // Read in the "atarixl.rom" file or use the built-in Altirra OS
    install_os();       // And install the right OS into our system...

    // ----------------------------------------------------------------------
    // Load the game file if supplied on the command line (mostly for TWL++)
    // ----------------------------------------------------------------------
    if(argc > 1) 
    {
        dsShowScreenMain();
        dsLoadGame(argv[1], 1, true, true);
        Atari800_Initialise();
        emu_state = A8_PLAYINIT;
    }
    else
    {
        chdir("/roms");    // Try to start in roms area... doesn't matter if it fails
        chdir("a800");     // And try to start in the subdir /a800... doesn't matter if it fails.
    }
    
    // Main loop of emulation
    dsMainLoop();
  
    // Free memory on the way out...
    dsFreeEmu();
    return(0);
}

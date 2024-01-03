/*
 * MAIN.C contains the entry point for the ARM9 execution.
 * 
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2024 Dave Bernazzani (wavemotion-dave)

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
#include "highscore.h"

extern void load_os(void);
extern void install_os(void);

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

    // Init FAT for file system use...
    if (!fatInitDefault()) 
    {
        iprintf("Unable to initialize libfat!\n");
        return -1;
    }

    // Get these in before we load configuration 
    load_os();          // Read in the "atarixl.rom" file or use the built-in Altirra OS
    install_os();       // And install the right OS into our system...
    
    // Intro and main screen
    intro_logo();  
    dsInitScreenMain();
    emu_state = A8_MENUINIT;
    
    highscore_init();

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
        chdir("a8");       // if the above failed, try a8
    }
    
    // Main loop of emulation
    dsMainLoop();
  
    // Free memory on the way out...
    dsFreeEmu();
    return(0);
}

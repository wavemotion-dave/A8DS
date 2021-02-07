#include <stdio.h>
#include <fat.h>
#include <nds.h>

#include <unistd.h>

#include "main.h"
#include "atari.h"
#include "intro.h"
#include "8bitutils.h"

extern int bg0, bg1;
int bg2, bg3;             // BG pointers 
int bg0s, bg1s, bg2s, bg3s;         // sub BG pointers 
extern volatile u16 vusCptVBL;             // VBL test

extern void load_os(void);
extern void install_os(void);

void irqVBlank(void) { 
  // Manage time
  vusCptVBL++;
}

// Program entry point
int main(int argc, char **argv) 
{
    // Init sound
    consoleDemoInit();
    soundEnable();
    lcdMainOnTop();

    // Init Fat
    if (!fatInitDefault()) 
    {
        iprintf("Unable to initialize libfat!\n");
        return -1;
    }

    // Init Timer
    dsInitTimer();
    dsInstallSoundEmuFIFO();

    // Intro and main screen
    intro_logo();  
    dsInitScreenMain();
    etatEmu = A5200_MENUINIT;

    load_os();          // Read in the "atarixl.rom" file or use the built-in Altirra OS
    install_os();       // And install the right OS into our system...

    // Load the game file if supplied on the command line (mostly for TWL++)
    if(argc > 1) 
    {
        dsShowScreenMain();
        dsLoadGame(argv[1], 1, true, true);
        Atari800_Initialise();
        etatEmu = A5200_PLAYINIT;
    }
    // Main loop of emulation
    dsMainLoop();
  
    // Free memory on the way out...
    dsFreeEmu();
    return(0);
}

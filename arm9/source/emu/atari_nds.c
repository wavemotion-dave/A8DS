/*
 * ATARI_NDS.C contains some Nintendo DS specific handling to bridge the
 * more generic Atari800 sources to the actual NDS hardware.  This used to 
 * be more useful when we didn't do a lot of NDS specific stuff directly in
 * the code which helped with gaining more raw speed for emulation. At some
 * point, this little bit of code can be subsumed into the main code and this
 * file can go away...
 *
 * XEGS-DS - Atari 8-bit Emulator designed to run 8-bit games on the Nintendo DS/DSi
 * Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "atari.h"
#include "cpu.h"
#include "input.h"
#include "pokeysnd.h"

u8 trig0 = 1;
u8 trig1 = 1;
u8 stick0 = STICK_CENTRE;
u8 stick1 = STICK_CENTRE;

/* -------------------------------------------------------------------------- */
/* CONFIG & INITIALISATION                                                    */
/* -------------------------------------------------------------------------- */
void Atari_Initialise(void) 
{
    // initialise sound routines 
    Pokey_sound_init(FREQ_17_APPROX, SOUND_FREQ, 1, 0);
    trig0 = 1;
    stick0 = 15;
    key_consol = CONSOL_NONE;
}

/* -------------------------------------------------------------------------- */
/* ATARI EXIT                                                                 */
/* -------------------------------------------------------------------------- */
int Atari_Exit(int run_monitor) 
{
    return 0;
}


/*
 * ATARI_NDS.C contains some Nintendo DS specific handling to bridge the
 * more generic Atari800 sources to the actual NDS hardware.  This used to 
 * be more useful when we didn't do a lot of NDS specific stuff directly in
 * the code which helped with gaining more raw speed for emulation. At some
 * point, this little bit of code can be subsumed into the main code and this
 * file can go away...
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


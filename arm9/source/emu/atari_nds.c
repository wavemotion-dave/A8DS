/* $Id: atari_vga.c,v 1.1 2001/10/26 05:36:48 fox Exp $ */
/* -------------------------------------------------------------------------- */

/*
 * DJGPP - VGA Backend for David Firth's Atari 800 emulator
 *
 * by Ivo van Poorten (C)1996  See file COPYRIGHT for policy.
 *
 */

/* -------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "config.h"
#include "cpu.h"
#include "input.h"
#include "sound.h"


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
    Sound_Initialise();
    trig0 = 1;
    stick0 = 15;
    key_consol = CONSOL_NONE;
}

/* -------------------------------------------------------------------------- */
/* ATARI EXIT                                                                 */
/* -------------------------------------------------------------------------- */
int Atari_Exit(int run_monitor) 
{
    Sound_Exit();
    return 0;
}


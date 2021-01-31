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
#include "pcjoy.h"
#include "rt-config.h"	/* for refresh_rate */
#include "screen.h"

u32 trig0 = 1;
u32 trig1 = 1;
u32 stick0 = STICK_CENTRE;
u32 stick1 = STICK_CENTRE;

/* this should be variables if we could move 320x200 window, but we can't :) */
/* static int first_lno = 24;
static int first_col = 32; */
#define first_lno 20 /* center 320x200 window in 384x240 screen */
#define first_col 32

/* -------------------------------------------------------------------------- */
/* CONFIG & INITIALISATION                                                    */
/* -------------------------------------------------------------------------- */
void Atari_Initialise(void) {
#ifdef DEBUG
  iprintf("Atari_Initialise\n");
#endif

#ifdef SOUND
  // initialise sound routines 
  Sound_Initialise();
#endif

	trig0 = 1;
	stick0 = 15;
	key_consol = CONSOL_NONE;
}

/* -------------------------------------------------------------------------- */
/* ATARI EXIT                                                                 */
/* -------------------------------------------------------------------------- */
int Atari_Exit(int run_monitor) {
#ifdef SOUND
  Sound_Exit();
#endif
  return 0;
}

/* -------------------------------------------------------------------------- */
u32 Atari_PORT(u32 num) {
	if (num == 0)
		return (stick1 << 4) | stick0;
	return 0xff;
}

/* -------------------------------------------------------------------------- */
u32 Atari_TRIG(u32 num) {
  switch (num) {
    case 0:
      return trig0;
    case 1:
      return trig1;
    default:
      return 1;
  }
}

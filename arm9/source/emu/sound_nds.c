/* $Id: sound_dos.c,v 1.2 2001/04/08 06:01:02 knik Exp $ */
#include <nds.h>

#include "atari.h"
#include "main.h"
#include "config.h"
#include "pokeysnd.h"

void Sound_Initialise(void) 
{
  Pokey_sound_init(FREQ_17_APPROX, SOUND_FREQ, 1, 0);
}

void Sound_Pause(void) {}

void Sound_Continue(void) {}

void Sound_Update(void) {}

void Sound_Exit(void) {}


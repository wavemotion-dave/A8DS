/*
 * pokeysnd.c - POKEY sound chip emulation, v2.4
 *
 * Copyright (C) 1996-1998 Ron Fries
 * Copyright (C) 1998-2006 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <nds.h>

#include "atari.h"
#include "pokeysnd.h"

#define READ_U32(x) \
  ((*(unsigned char *) (x)) | ((*((unsigned char *) (x) + 1)) << 8) | \
  ((*((unsigned char *) (x) + 2)) << 16) | ((*((unsigned char *) (x) + 3)) << 24))
#define WRITE_U32(x, d) \
  { \
  uint32 i = d; \
  (*(unsigned char *)(x)) = ((i) & 255); \
  (*((unsigned char *)(x) + 1)) = (((i) >> 8) & 255); \
  (*((unsigned char *)(x) + 2)) = (((i) >> 16) & 255); \
  (*((unsigned char *)(x) + 3)) = (((i) >> 24) & 255); \
  }

/* GLOBAL VARIABLE DEFINITIONS */
extern int debug[];

/* number of pokey chips currently emulated */
static uint8 Num_pokeys;

static uint8 AUDV[4 * MAXPOKEYS] __attribute__((section(".dtcm")));	/* Channel volume - derived */

static uint8 Outbit[4 * MAXPOKEYS] __attribute__((section(".dtcm")));		/* current state of the output (high or low) */

static uint8 Outvol[4 * MAXPOKEYS] __attribute__((section(".dtcm")));		/* last output volume for each channel */

/* Initialze the bit patterns for the polynomials. */

/* The 4bit and 5bit patterns are the identical ones used in the pokey chip. */
/* Though the patterns could be packed with 8 bits per byte, using only a */
/* single bit per byte keeps the math simple, which is important for */
/* efficient processing. */

static uint8 bit4[POLY4_SIZE] __attribute__((section(".dtcm"))) =
{1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0};	/* new table invented by Perry */

static uint8 bit5[POLY5_SIZE] __attribute__((section(".dtcm"))) =
{1, 1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0};

static uint32 P4 = 0,			/* Global position pointer for the 4-bit  POLY array */
 P5 = 0,						/* Global position pointer for the 5-bit  POLY array */
 P9 = 0,						/* Global position pointer for the 9-bit  POLY array */
 P17 = 0;						/* Global position pointer for the 17-bit POLY array */

static uint32 Div_n_cnt[4 * MAXPOKEYS] __attribute__((section(".dtcm")));		/* Divide by n counter. one for each channel */
static uint32 Div_n_max[4 * MAXPOKEYS] __attribute__((section(".dtcm")));		/* Divide by n maximum, one for each channel */

static uint32 Samp_n_max,		/* Sample max.  For accuracy, it is *256 */
 Samp_n_cnt[2] __attribute__ ((aligned (4)));					/* Sample cnt. */

extern int sound_quality;
/* Volume only emulations declarations */
static uint32 snd_freq17 = FREQ_17_EXACT;
int32 snd_playback_freq = 44100;
uint8 snd_num_pokeys = 1;
static int snd_flags = 0;

/* multiple sound engine interface */
static void null_pokey_process(void *sndbuffer, unsigned int sndn) {}
void (*Pokey_process_ptr)(void *sndbuffer, unsigned int sndn) = null_pokey_process;

static void Update_pokey_sound_rf(uint16, uint8, uint8, uint8);
static void null_pokey_sound(uint16 addr, uint8 val, uint8 chip, uint8 gain) {}
void (*Update_pokey_sound) (uint16 addr, uint8 val, uint8 chip, uint8 gain) = null_pokey_sound;

/*****************************************************************************/
/* In my routines, I treat the sample output as another divide by N counter  */
/* For better accuracy, the Samp_n_cnt has a fixed binary decimal point      */
/* which has 8 binary digits to the right of the decimal point.  I use a two */
/* byte array to give me a minimum of 40 bits, and then use pointer math to  */
/* reference either the 24.8 whole/fraction combination or the 32-bit whole  */
/* only number.  This is mainly used to keep the math simple for             */
/* optimization. See below:                                                  */
/*                                                                           */
/* Representation on little-endian machines:                                 */
/* xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx */
/* fraction   whole    whole    whole      whole   unused   unused   unused  */
/*                                                                           */
/* Samp_n_cnt[0] gives me a 32-bit int 24 whole bits with 8 fractional bits, */
/* while (uint32 *)((uint8 *)(&Samp_n_cnt[0])+1) gives me the 32-bit whole   */
/* number only.                                                              */
/*                                                                           */
/* Representation on big-endian machines:                                    */
/* xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx | xxxxxxxx xxxxxxxx xxxxxxxx.xxxxxxxx */
/*  unused   unused   unused    whole      whole    whole    whole  fraction */
/*                                                                           */
/* Samp_n_cnt[1] gives me a 32-bit int 24 whole bits with 8 fractional bits, */
/* while (uint32 *)((uint8 *)(&Samp_n_cnt[0])+3) gives me the 32-bit whole   */
/* number only.                                                              */
/*****************************************************************************/


/*****************************************************************************/
/* Module:  Pokey_sound_init()                                               */
/* Purpose: to handle the power-up initialization functions                  */
/*          these functions should only be executed on a cold-restart        */
/*                                                                           */
/* Author:  Ron Fries                                                        */
/* Date:    January 1, 1997                                                  */
/*                                                                           */
/* Inputs:  freq17 - the value for the '1.79MHz' Pokey audio clock           */
/*          playback_freq - the playback frequency in samples per second     */
/*          num_pokeys - specifies the number of pokey chips to be emulated  */
/*                                                                           */
/* Outputs: Adjusts local globals - no return value                          */
/*                                                                           */
/*****************************************************************************/

static int Pokey_sound_init_rf(uint32 freq17, uint16 playback_freq, uint8 num_pokeys, unsigned int flags)
{
	uint8 chan;

	Update_pokey_sound = Update_pokey_sound_rf;

	/* start all of the polynomial counters at zero */
	P4 = 0;
	P5 = 0;
	P9 = 0;
	P17 = 0;

	/* calculate the sample 'divide by N' value based on the playback freq. */
	Samp_n_max = ((uint32) freq17 << 8) / playback_freq;

	Samp_n_cnt[0] = 0;			/* initialize all bits of the sample */
	Samp_n_cnt[1] = 0;			/* 'divide by N' counter */

	for (chan = 0; chan < (MAXPOKEYS * 4); chan++) {
		Outvol[chan] = 0;
		Outbit[chan] = 0;
		Div_n_cnt[chan] = 0;
		Div_n_max[chan] = 0x7fffffffL;
		AUDV[chan] = 0;
	}

	/* set the number of pokey chips currently emulated */
	Num_pokeys = num_pokeys;

	return 0; /* OK */
}

int Pokey_DoInit(void) 
{
    return Pokey_sound_init_rf(snd_freq17, (uint16) snd_playback_freq, snd_num_pokeys, snd_flags);
}

int  Pokey_sound_init(uint32 freq17, uint16 playback_freq, uint8 num_pokeys, unsigned int flags)
{
	snd_freq17 = freq17;
	snd_playback_freq = playback_freq;
	snd_num_pokeys = num_pokeys;
	snd_flags = flags;

	return Pokey_DoInit();
}

/*****************************************************************************/
/* Module:  Update_pokey_sound()                                             */
/* Purpose: To process the latest control values stored in the AUDF, AUDC,   */
/*          and AUDCTL registers.  It pre-calculates as much information as  */
/*          possible for better performance.  This routine has not been      */
/*          optimized.                                                       */
/*                                                                           */
/* Author:  Ron Fries                                                        */
/* Date:    January 1, 1997                                                  */
/*                                                                           */
/* Inputs:  addr - the address of the parameter to be changed                */
/*          val - the new value to be placed in the specified address        */
/*          gain - specified as an 8-bit fixed point number - use 1 for no   */
/*                 amplification (output is multiplied by gain)              */
/*                                                                           */
/* Outputs: Adjusts local globals - no return value                          */
/*                                                                           */
/*****************************************************************************/

static void Update_pokey_sound_rf(uint16 addr, uint8 val, uint8 chip, uint8 gain) 
{
	uint32 new_val = 0;
	uint8 chan;
	uint8 chan_mask;

	/* determine which address was changed */
	switch (addr & 0x0f) {
	case _AUDF1:
		chan_mask = 1 << CHAN1;
		if (AUDCTL[0] & CH1_CH2)		/* if ch 1&2 tied together */
			chan_mask |= 1 << CHAN2;	/* then also change on ch2 */
		break;
	case _AUDC1:
		AUDV[CHAN1] = (val & VOLUME_MASK) * gain;
		chan_mask = 1 << CHAN1;
		break;
	case _AUDF2:
		chan_mask = 1 << CHAN2;
		break;
	case _AUDC2:
		AUDV[CHAN2] = (val & VOLUME_MASK) * gain;
		chan_mask = 1 << CHAN2;
		break;
	case _AUDF3:
		chan_mask = 1 << CHAN3;
		if (AUDCTL[0] & CH3_CH4)		/* if ch 3&4 tied together */
			chan_mask |= 1 << CHAN4;	/* then also change on ch4 */
		break;
	case _AUDC3:
		AUDV[CHAN3] = (val & VOLUME_MASK) * gain;
		chan_mask = 1 << CHAN3;
		break;
	case _AUDF4:
		chan_mask = 1 << CHAN4;
		break;
	case _AUDC4:
		AUDV[CHAN4] = (val & VOLUME_MASK) * gain;
		chan_mask = 1 << CHAN4;
		break;
	case _AUDCTL:
		chan_mask = 15;			/* all channels */
		break;
	default:
		chan_mask = 0;
		break;
	}

	/************************************************************/
	/* As defined in the manual, the exact Div_n_cnt values are */
	/* different depending on the frequency and resolution:     */
	/*    64 kHz or 15 kHz - AUDF + 1                           */
	/*    1 MHz, 8-bit -     AUDF + 4                           */
	/*    1 MHz, 16-bit -    AUDF[CHAN1]+256*AUDF[CHAN2] + 7    */
	/************************************************************/

	/* only reset the channels that have changed */

	if (chan_mask & (1 << CHAN1)) {
		/* process channel 1 frequency */
		if (AUDCTL[0] & CH1_179)
			new_val = AUDF[CHAN1] + 4;
		else
			new_val = (AUDF[CHAN1] + 1) * Base_mult[0];

		if (new_val != Div_n_max[CHAN1]) {
			Div_n_max[CHAN1] = new_val;

			if (Div_n_cnt[CHAN1] > new_val) {
				Div_n_cnt[CHAN1] = new_val;
			}
		}
	}

	if (chan_mask & (1 << CHAN2)) {
		/* process channel 2 frequency */
		if (AUDCTL[0] & CH1_CH2) {
			if (AUDCTL[0] & CH1_179)
				new_val = AUDF[CHAN2] * 256 +
					AUDF[CHAN1] + 7;
			else
				new_val = (AUDF[CHAN2] * 256 +
						   AUDF[CHAN1] + 1) * Base_mult[0];
		}
		else
			new_val = (AUDF[CHAN2] + 1) * Base_mult[0];

		if (new_val != Div_n_max[CHAN2]) {
			Div_n_max[CHAN2] = new_val;

			if (Div_n_cnt[CHAN2] > new_val) {
				Div_n_cnt[CHAN2] = new_val;
			}
		}
	}

	if (chan_mask & (1 << CHAN3)) {
		/* process channel 3 frequency */
		if (AUDCTL[0] & CH3_179)
			new_val = AUDF[CHAN3] + 4;
		else
			new_val = (AUDF[CHAN3] + 1) * Base_mult[0];

		if (new_val != Div_n_max[CHAN3]) {
			Div_n_max[CHAN3] = new_val;

			if (Div_n_cnt[CHAN3] > new_val) {
				Div_n_cnt[CHAN3] = new_val;
			}
		}
	}

	if (chan_mask & (1 << CHAN4)) {
		/* process channel 4 frequency */
		if (AUDCTL[0] & CH3_CH4) {
			if (AUDCTL[0] & CH3_179)
				new_val = AUDF[CHAN4] * 256 +
					AUDF[CHAN3] + 7;
			else
				new_val = (AUDF[CHAN4] * 256 +
						   AUDF[CHAN3] + 1) * Base_mult[0];
		}
		else
			new_val = (AUDF[CHAN4] + 1) * Base_mult[0];

		if (new_val != Div_n_max[CHAN4]) {
			Div_n_max[CHAN4] = new_val;

			if (Div_n_cnt[CHAN4] > new_val) {
				Div_n_cnt[CHAN4] = new_val;
			}
		}
	}

	/* if channel is volume only, set current output */
	for (chan = CHAN1; chan <= CHAN4; chan++) 
    {
		if (chan_mask & (1 << chan)) 
        {
			/* I've disabled any frequencies that exceed the sampling
			   frequency.  There isn't much point in processing frequencies
			   that the hardware can't reproduce.  I've also disabled
			   processing if the volume is zero. */
            if ((AUDC[chan] & VOL_ONLY) || ((AUDC[chan] & VOLUME_MASK) == 0) || (Div_n_max[chan] < (Samp_n_max >> 8)))
            {
				/* indicate the channel is 'on' */
				Outvol[chan] = 1;
                /* and set channel freq to max to reduce processing */
                Div_n_max[chan] = 0x7fffffffL;
                Div_n_cnt[chan] = 0x7fffffffL;
			}
		}
	}
}


/*****************************************************************************/
/* Module:  Pokey_process()                                                  */
/* Purpose: To fill the output buffer with the sound output based on the     */
/*          pokey chip parameters.                                           */
/*                                                                           */
/* Author:  Ron Fries                                                        */
/* Date:    January 1, 1997                                                  */
/*                                                                           */
/* Inputs:  *buffer - pointer to the buffer where the audio output will      */
/*                    be placed                                              */
/*          n - size of the playback buffer                                  */
/*                                                                           */
/* Outputs: the buffer will be filled with n bytes of audio - no return val  */
/*                                                                           */
/*****************************************************************************/
void Pokey_process(void *sndbuffer, unsigned sndn)
{
	register char *buffer = (char  *) sndbuffer;
	register uint16 n = sndn;

	register uint32 *div_n_ptr;
	register uint8 *samp_cnt_w_ptr;
	register uint32 event_min;
	register uint8 next_event;
	register unsigned char cur_val;		/* otherwise we'll simplify as 8-bit unsigned */
	register uint8 *out_ptr;
	register uint8 audc;
	register uint8 toggle;
	register uint8 *vol_ptr;
  
	/* set a pointer to the whole portion of the samp_n_cnt */
	samp_cnt_w_ptr = ((uint8 *) (&Samp_n_cnt[0]) + 1);

	/* set a pointer for optimization */
	out_ptr = Outvol;
	vol_ptr = AUDV;

	/* The current output is pre-determined and then adjusted based on each */
	/* output change for increased performance (less over-all math). */
	/* add the output values of all 4 channels */
	cur_val = SAMP_MIN;
    if (*out_ptr++) cur_val += *vol_ptr;
    vol_ptr++;

    if (*out_ptr++) cur_val += *vol_ptr;
    vol_ptr++;

    if (*out_ptr++) cur_val += *vol_ptr;
    vol_ptr++;

    if (*out_ptr++) cur_val += *vol_ptr;
    vol_ptr++;

	/* loop until the buffer is filled */
    while (n) 
    {
		/* Normally the routine would simply decrement the 'div by N' */
		/* counters and react when they reach zero.  Since we normally */
		/* won't be processing except once every 80 or so counts, */
		/* I've optimized by finding the smallest count and then */
		/* 'accelerated' time by adjusting all pointers by that amount. */

		/* find next smallest event (either sample or chan 1-4) */
		next_event = SAMPLE;
		event_min = READ_U32(samp_cnt_w_ptr);

		div_n_ptr = Div_n_cnt;

    /* Though I could have used a loop here, this is faster */
    if (*div_n_ptr <= event_min) {
        event_min = *div_n_ptr;
        next_event = CHAN1;
    }
    div_n_ptr++;
    if (*div_n_ptr <= event_min) {
        event_min = *div_n_ptr;
        next_event = CHAN2;
    }
    div_n_ptr++;
    if (*div_n_ptr <= event_min) {
        event_min = *div_n_ptr;
        next_event = CHAN3;
    }
    div_n_ptr++;
    if (*div_n_ptr <= event_min) {
        event_min = *div_n_ptr;
        next_event = CHAN4;
    }
    div_n_ptr++;

		/* if the next event is a channel change */
		if (next_event != SAMPLE) {
			/* shift the polynomial counters */

      /* decrement all counters by the smallest count found */
      /* again, no loop for efficiency */
      div_n_ptr--;
      *div_n_ptr -= event_min;
      div_n_ptr--;
      *div_n_ptr -= event_min;
      div_n_ptr--;
      *div_n_ptr -= event_min;
      div_n_ptr--;
      *div_n_ptr -= event_min;

			WRITE_U32(samp_cnt_w_ptr, READ_U32(samp_cnt_w_ptr) - event_min);

			/* since the polynomials require a mod (%) function which is
			   division, I don't adjust the polynomials on the SAMPLE events,
			   only the CHAN events.  I have to keep track of the change,
			   though. */

			P4 = (P4 + event_min) % POLY4_SIZE;
			P5 = (P5 + event_min) % POLY5_SIZE;
			P9 = (P9 + event_min) % POLY9_SIZE;
			P17 = (P17 + event_min) % POLY17_SIZE;

			/* adjust channel counter */
			Div_n_cnt[next_event] += Div_n_max[next_event];

			/* get the current AUDC into a register (for optimization) */
			audc = AUDC[next_event];

			/* set a pointer to the current output (for opt...) */
			out_ptr = &Outvol[next_event];

			/* assume no changes to the output */
			toggle = FALSE;

			/* From here, a good understanding of the hardware is required */
			/* to understand what is happening.  I won't be able to provide */
			/* much description to explain it here. */

			/* if VOLUME only then nothing to process */
			if (!(audc & VOL_ONLY)) {
				/* if the output is pure or the output is poly5 and the poly5 bit */
				/* is set */
				if ((audc & NOTPOLY5) || bit5[P5]) {
					/* if the PURETONE bit is set */
					if (audc & PURETONE) {
						/* then simply toggle the output */
						toggle = TRUE;
					}
					/* otherwise if POLY4 is selected */
					else if (audc & POLY4) {
						/* then compare to the poly4 bit */
						toggle = (bit4[P4] == !(*out_ptr));
					}
					else {
						/* if 9-bit poly is selected on this chip */
						if (AUDCTL[next_event >> 2] & POLY9) {
							/* compare to the poly9 bit */
							toggle = ((poly9_lookup[P9] & 1) == !(*out_ptr));
						}
						else {
							/* otherwise compare to the poly17 bit */
							toggle = (((poly17_lookup[P17 >> 3] >> (P17 & 7)) & 1) == !(*out_ptr));
						}
					}
				}
			}

			/* check channel 1 filter (clocked by channel 3) */
			if ( AUDCTL[next_event >> 2] & CH1_FILTER) {
				/* if we're processing channel 3 */
				if ((next_event & 0x03) == CHAN3) {
					/* check output of channel 1 on same chip */
					if (Outvol[next_event & 0xfd]) {
						/* if on, turn it off */
						Outvol[next_event & 0xfd] = 0;
						cur_val -= AUDV[next_event & 0xfd];
					}
				}
			}

			/* check channel 2 filter (clocked by channel 4) */
			if ( AUDCTL[next_event >> 2] & CH2_FILTER) {
				/* if we're processing channel 4 */
				if ((next_event & 0x03) == CHAN4) {
					/* check output of channel 2 on same chip */
					if (Outvol[next_event & 0xfd]) {
						/* if on, turn it off */
						Outvol[next_event & 0xfd] = 0;
						cur_val -= AUDV[next_event & 0xfd];
					}
				}
			}

			/* if the current output bit has changed */
			if (toggle) {
				if (*out_ptr) {
					/* remove this channel from the signal */
					cur_val -= AUDV[next_event];

					/* and turn the output off */
					*out_ptr = 0;
				}
				else {
					/* turn the output on */
					*out_ptr = 1;

					/* and add it to the output signal */
					cur_val += AUDV[next_event];
				}
			}
		}
		else {					/* otherwise we're processing a sample */
			/* adjust the sample counter - note we're using the 24.8 integer
			   which includes an 8 bit fraction for accuracy */
      int iout;
      iout = cur_val;
      *buffer++ = (char) ((iout))+128;
      *Samp_n_cnt += Samp_n_max;
      /* and indicate one less byte in the buffer */
      n--;
		}    
	}
}


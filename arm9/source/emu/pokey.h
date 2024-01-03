/*
 * POKEY.C contains the emulation of the Pokey sound chip on the Atari 800. 
 * 
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly recommended you seek out the latest Atari800 sources.
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
 * be distributed using that same licensing model. See COPYING for the full license
 * but the original Atari800 copyright notice retained below:
 */

/*
 * pokey.c - POKEY sound chip emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2008 Atari800 development team (see DOC/CREDITS)
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

#ifndef _POKEY_H_
#define _POKEY_H_

#ifdef ASAP /* external project, see http://asap.sf.net */
#include "asap_internal.h"
#else
#include "atari.h"
#endif

#define _AUDF1 0x00
#define _AUDC1 0x01
#define _AUDF2 0x02
#define _AUDC2 0x03
#define _AUDF3 0x04
#define _AUDC3 0x05
#define _AUDF4 0x06
#define _AUDC4 0x07
#define _AUDCTL 0x08
#define _STIMER 0x09
#define _SKRES 0x0a
#define _POTGO 0x0b
#define _SEROUT 0x0d
#define _IRQEN 0x0e
#define _SKCTLS 0x0f

#define _POT0 0x00
#define _POT1 0x01
#define _POT2 0x02
#define _POT3 0x03
#define _POT4 0x04
#define _POT5 0x05
#define _POT6 0x06
#define _POT7 0x07
#define _ALLPOT 0x08
#define _KBCODE 0x09
#define _RANDOM 0x0a
#define _SERIN 0x0d
#define _IRQST 0x0e
#define _SKSTAT 0x0f

#define _POKEY2 0x10            /* offset to second pokey chip (STEREO expansion) */

#ifndef ASAP


#define POKEY_DELAYED_SERIN_IRQ  DELAYED_SERIN_IRQ
#define POKEY_AUDF AUDF
#define POKEY_AUDC AUDC
#define POKEY_CHAN3 CHAN3
#define POKEY_CHAN4 CHAN4

ULONG POKEY_GetRandomCounter(void);
void POKEY_SetRandomCounter(ULONG value);
UBYTE POKEY_GetByte(UWORD addr);
void POKEY_PutByte(UWORD addr, UBYTE byte);
void POKEY_Initialise(void);
void POKEY_Frame(void);
void POKEY_Scanline(void);

#endif

/* CONSTANT DEFINITIONS */

/* definitions for AUDCx (D201, D203, D205, D207) */
#define NOTPOLY5    0x80        /* selects POLY5 or direct CLOCK */
#define POLY4       0x40        /* selects POLY4 or POLY17 */
#define PURETONE    0x20        /* selects POLY4/17 or PURE tone */
#define VOL_ONLY    0x10        /* selects VOLUME OUTPUT ONLY */
#define VOLUME_MASK 0x0f        /* volume mask */

/* definitions for AUDCTL (D208) */
#define POLY9       0x80        /* selects POLY9 or POLY17 */
#define CH1_179     0x40        /* selects 1.78979 MHz for Ch 1 */
#define CH3_179     0x20        /* selects 1.78979 MHz for Ch 3 */
#define CH1_CH2     0x10        /* clocks channel 1 w/channel 2 */
#define CH3_CH4     0x08        /* clocks channel 3 w/channel 4 */
#define CH1_FILTER  0x04        /* selects channel 1 high pass filter */
#define CH2_FILTER  0x02        /* selects channel 2 high pass filter */
#define CLOCK_15    0x01        /* selects 15.6999kHz or 63.9210kHz */

/* for accuracy, the 64kHz and 15kHz clocks are exact divisions of
   the 1.79MHz clock */
#define DIV_64      28          /* divisor for 1.79MHz clock to 64 kHz */
#define DIV_15      114         /* divisor for 1.79MHz clock to 15 kHz */

/* the size (in entries) of the 4 polynomial tables */
#define POLY4_SIZE  0x000f
#define POLY5_SIZE  0x001f
#define POLY9_SIZE  0x01ff
#define POLY17_SIZE 0x0001ffff

#define MAXPOKEYS    2      /* max number of emulated chips - we support 2 for STEREO sound */

/* channel/chip definitions */
#define CHAN1       0
#define CHAN2       1
#define CHAN3       2
#define CHAN4       3
#define CHIP1       0
#define CHIP2       4
#define CHIP3       8
#define CHIP4      12
#define SAMPLE    127

extern unsigned short pokeyBufIdx;
extern char pokey_buffer[SNDLENGTH];
extern UBYTE KBCODE;
extern UBYTE SERIN;
extern UBYTE IRQST;
extern UBYTE IRQEN;
extern UBYTE SKSTAT;
extern UBYTE SKCTLS;
extern int DELAYED_SERIN_IRQ;
extern int DELAYED_SEROUT_IRQ;
extern int DELAYED_XMTDONE_IRQ;
extern UBYTE AUDF[4 * MAXPOKEYS];
extern UBYTE AUDC[4 * MAXPOKEYS];
extern UBYTE AUDCTL[MAXPOKEYS];
extern int DivNIRQ[4];
extern int DivNMax[4];
extern int Base_mult[MAXPOKEYS];
extern UBYTE POT_input[8];
extern UBYTE PCPOT_input[8];
extern UBYTE POT_all;
extern UBYTE pot_scanline;
extern ULONG random_scanline_counter;
extern UBYTE poly9_lookup[511];
extern UBYTE poly17_lookup[16385];


#endif

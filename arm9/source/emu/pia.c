/*
 * PIA.C contains the emulation of the PIA (Paralell) chip interface.
 * The baseline for this file is the Atari800 4.20 source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * The original Atari800 copyright message is retained below.
 *
 * XEGS-DS - Atari 8-bit Emulator designed to run 8-bit games on the Nintendo DS/DSi
 * Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */

/*
 * pia.c - PIA chip emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "sio.h"

#ifndef BASIC
#include "input.h"
#include "statesav.h"
#endif

UBYTE PACTL __attribute__((section(".dtcm")));
UBYTE PBCTL __attribute__((section(".dtcm")));
UBYTE PORTA __attribute__((section(".dtcm")));
UBYTE PORTB __attribute__((section(".dtcm")));
UBYTE PORT_input[2] __attribute__((section(".dtcm")));

int xe_bank __attribute__((section(".dtcm")))= 0;
int selftest_enabled = 0;

UBYTE atari_basic[8192];
UBYTE atari_os[16384];

UBYTE PORTA_mask __attribute__((section(".dtcm")));
UBYTE PORTB_mask __attribute__((section(".dtcm")));

void PIA_Initialise(void) {
	PACTL = 0x3f;
	PBCTL = 0x3f;
	PORTA = 0xff;
	PORTB = 0xff;
	PORTA_mask = 0xff;
	PORTB_mask = 0xff;
	PORT_input[0] = 0xff;
	PORT_input[1] = 0xff;
}

void PIA_Reset(void) {
	PORTA = 0xff;
	if (machine_type == MACHINE_XLXE) {
		MEMORY_HandlePORTB(0xff, (UBYTE) (PORTB | PORTB_mask));
	}
	PORTB = 0xff;
}

UBYTE PIA_GetByte(UWORD addr) {
	switch (addr & 0x03) {
    case _PACTL:
      return PACTL & 0x3f;
    case _PBCTL:
      return PBCTL & 0x3f;
    case _PORTA:
      if ((PACTL & 0x04) == 0) {
        /* direction register */
        return ~PORTA_mask;
      }
      else {
        /* port state */
        return PORT_input[0] & (PORTA | PORTA_mask);
      }
    case _PORTB:
      if ((PBCTL & 0x04) == 0) {
        /* direction register */
        return ~PORTB_mask;
      }
      else {
        /* port state */
        if (machine_type == MACHINE_XLXE) {
          return PORTB | PORTB_mask;
        }
        else {
          return PORT_input[1] & (PORTB | PORTB_mask);
        }
      }
  }
	/* for stupid compilers */
	return 0xff;
}

void PIA_PutByte(UWORD addr, UBYTE byte) {
	switch (addr & 0x03) {
	case _PACTL:
                /* This code is part of the cassette emulation */
		if ((PACTL ^ byte) & 0x08) {
			/* The motor status has changed */
			SIO_TapeMotor(byte & 0x08 ? 0 : 1);
		}
		PACTL = byte;
		break;
	case _PBCTL:
		/* This code is part of the serial I/O emulation */
		if ((PBCTL ^ byte) & 0x08) {
			/* The command line status has changed */
			SIO_SwitchCommandFrame(byte & 0x08 ? 0 : 1);
		}
		PBCTL = byte;
		break;
	case _PORTA:
		if ((PACTL & 0x04) == 0) {
			/* set direction register */
 			PORTA_mask = ~byte;
		}
		else {
			/* set output register */
			PORTA = byte;		/* change from thor */
		}
		break;
	case _PORTB:
		if (machine_type == MACHINE_XLXE) {
			if ((PBCTL & 0x04) == 0) {
				/* direction register */
				MEMORY_HandlePORTB((UBYTE) (PORTB | ~byte), (UBYTE) (PORTB | PORTB_mask));
				PORTB_mask = ~byte;
			}
			else {
				/* output register */
				MEMORY_HandlePORTB((UBYTE) (byte | PORTB_mask), (UBYTE) (PORTB | PORTB_mask));
				PORTB = byte;
			}
		}
		else {
			if ((PBCTL & 0x04) == 0) {
				/* direction register */
				PORTB_mask = ~byte;
			}
			else {
				/* output register */
				PORTB = byte;
			}
		}
		break;
	}
}

void PIAStateSave(void) {}
void PIAStateRead(void) {}


/*
 * PIA.C contains the emulation of the PIA  chip interface.
 * 
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly ecommended that you seek out the latest Atari800 sources.
 *
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)

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

#include "atari.h"
#include "cpu.h"
#include "memory.h"
#include "pia.h"
#include "pokey.h"
#include "sio.h"
#include "input.h"

UBYTE PACTL         __attribute__((section(".dtcm")));
UBYTE PBCTL         __attribute__((section(".dtcm")));
UBYTE PORTA         __attribute__((section(".dtcm")));
UBYTE PORTB         __attribute__((section(".dtcm")));
UBYTE PIA_IRQ       __attribute__((section(".dtcm")));
UBYTE PORTA_mask    __attribute__((section(".dtcm")));
UBYTE PORTB_mask    __attribute__((section(".dtcm")));
UBYTE PORT_input[2] __attribute__((section(".dtcm")));

UBYTE PIA_CA2 = 1;
UBYTE PIA_CA2_negpending = 0;
UBYTE PIA_CA2_pospending = 0;
UBYTE PIA_CB2 = 1;
UBYTE PIA_CB2_negpending = 0;
UBYTE PIA_CB2_pospending = 0;
UBYTE PIA_IRQ = 0;

int xe_bank __attribute__((section(".dtcm")))= 0;
int selftest_enabled = 0;

UBYTE atari_os[0x4000];
UBYTE *atari_os_pristine = (UBYTE *)(0x06898000+0x4000);    // Steal 16K here of VRAM for pristine (unpatched) OS copy which we can memcpy() back to atari_os[] if needed

void PIA_Initialise(void) 
{
    PACTL = 0x3f;
    PBCTL = 0x3f;
    PORTA = 0xff;
    PORTB = 0xff;
    PORTA_mask = 0xff;
    PORTB_mask = 0xff;
    PORT_input[0] = 0xff;
    PORT_input[1] = 0xff;
    PIA_IRQ = 0;
    PIA_CA2 = 1;
    PIA_CA2_negpending = 0;
    PIA_CA2_pospending = 0;
    PIA_CB2 = 1;
    PIA_CB2_negpending = 0;
    PIA_CB2_pospending = 0;
}

void PIA_Reset(void) 
{
    PORTA = 0xff;
    if (myConfig.machine_type >= MACHINE_XLXE_64K) 
    {
        MEMORY_HandlePORTB(0xff, (UBYTE) (PORTB | PORTB_mask));
    }
    PORTB = 0xff;
    PIA_IRQ = 0;
}

static void set_CA2(int value)
{
	/* This code is part of the cassette emulation */
	if (PIA_CA2 != value) {
		/* The motor status has changed */
		SIO_TapeMotor(!value);
	}
	PIA_CA2 = value;
}

static void set_CB2(int value)
{
	/* This code is part of the serial I/O emulation */
	if (PIA_CB2 != value) {
		/* The command line status has changed */
		SIO_SwitchCommandFrame(!value);
	}
	PIA_CB2 = value;
}

static void update_PIA_IRQ(void)
{
	PIA_IRQ = 0;
	if (((PACTL & 0x40) &&  (PACTL & 0x28) == 0x08) || 
		 ((PBCTL & 0x40) && (PBCTL & 0x28) == 0x08) ||
		 ((PACTL & 0x80) && (PACTL & 0x01)) ||
		 ((PBCTL & 0x80) && (PBCTL & 0x01))) {
		PIA_IRQ = 1;
	}
	/* update pokey IRQ status */
	POKEY_PutByte(_IRQEN, IRQEN);
}

UBYTE PIA_GetByte(UWORD addr) {
	switch (addr & 0x03) {
	case _PACTL: 
		/* read CRA (control register A) */
		return PACTL;
	case _PBCTL: 
		/* read CRB (control register B) */
		return PBCTL;
	case _PORTA:
		if ((PACTL & 0x04) == 0) {
			/* read DDRA (data direction register A) */
			return ~PORTA_mask;
		}
		else {
			/* read PIBA (peripheral interface buffer A) */
			/* also called ORA (output register A) even for reading in data sheet */
            if (((PACTL & 0x38)>>3) == 0x04) { /* handshake */
                if (PIA_CA2 == 1) {
                    PIA_CA2_negpending = 1;
                }
                set_CA2(0);
            }
            else if (((PACTL & 0x38)>>3) == 0x05) { /* pulse */
                set_CA2(0);
                set_CA2(1); /* FIXME one cycle later ... */
            }
            PACTL &= 0x3f; /* clear bit 6 & 7 */
            update_PIA_IRQ();

			return PORT_input[0] & (PIA_PORTA | PORTA_mask);
		}
	case _PORTB:
		if ((PBCTL & 0x04) == 0) {
			/* read DDRA (data direction register B) */
			return ~PORTB_mask;
		}
		else {
			/* read PIBB (peripheral interface buffer B) */
			/* also called ORB (output register B) even for reading in data sheet */
            PBCTL &= 0x3f; /* clear bit 6 & 7 */
            update_PIA_IRQ();

			if (myConfig.machine_type >= MACHINE_XLXE_64K) 
            {
				return PIA_PORTB | PORTB_mask;
			}
			else 
            {
				return PORT_input[1] & (PIA_PORTB | PORTB_mask);
			}
		}
	}
	/* for stupid compilers */
	return 0xff;
}

void PIA_PutByte(UWORD addr, UBYTE byte) {
	switch (addr & 0x03) {
	case _PACTL: 
		/* write CRA (control register A) */
		byte &= 0x3f; /* bits 6 & 7 cannot be written */
	
		PACTL = ((PACTL & 0xc0) | byte);
		/* CA2 control */
		switch((byte & 0x38)>>3) {
			case 0: /* 000 input, negative transition, no irq*/
			case 1: /* 001 input, negative transition, irq */
				if (PIA_CA2_negpending) {
					PACTL |= 0x40;
				}
				set_CA2(1);
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 2: /* 010 input, positive transition, no irq */
			case 3: /* 011 input, positive transition, irq */
				if (!PIA_CA2 || PIA_CA2_pospending) {
					PACTL |= 0x40;
				}
				set_CA2(1);
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 4: /* 100 output, handshake mode */
				PIA_CA2_pospending = 0;
				PACTL &= 0x3f;
			break;
			case 5: /* 101 output, pulse mode */
				set_CA2(1); /* FIXME after one cycle, if 0 */
				PACTL &= 0x3f;
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 6: /* 110 output low */
				set_CA2(0);
				PACTL &= 0x3f;
				PIA_CA2_negpending = 0;
				PIA_CA2_pospending = 0;
			break;
			case 7: /* 111 output high */
				PACTL &= 0x3f;
				if (!PIA_CA2_negpending && PIA_CA2 == 0) {
					PIA_CA2_pospending = 1;
				}
				set_CA2(1);
				PIA_CA2_negpending = 0;
			break;
		}
		update_PIA_IRQ();
		break;
	case _PBCTL: 
		/* write CRB (control register B) */
		byte &= 0x3f; /* bits 6 & 7 cannot be written */
		PBCTL = ((PBCTL & 0xc0) | byte);
		/* CB2 control */
		switch((byte & 0x38)>>3) {
			case 0: /* 000 input, negative transition, no irq*/
			case 1: /* 001 input, negative transition, irq */
				/* PACTL has: if (PIA_CB2_negpending) */
				if (PIA_CB2_negpending || PIA_CB2_pospending) {
					PBCTL |= 0x40;
				}
				set_CB2(1);
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 2: /* 010 input, positive transition, no irq */
			case 3: /* 011 input, positive transition, irq */
				if (!PIA_CB2 || PIA_CB2_pospending) {
					PBCTL |= 0x40;
				}
				set_CB2(1);
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 4: /* 100 output, handshake mode */
				PIA_CB2_pospending = 0;
				PBCTL &= 0x3f;
			break;
			case 5: /* 101 output, pulse mode */
				set_CB2(1); /* FIXME after one cycle, if 0 */
				PBCTL &= 0x3f;
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 6: /* 110 output low */
				set_CB2(0);
				PBCTL &= 0x3f;
				PIA_CB2_negpending = 0;
				PIA_CB2_pospending = 0;
			break;
			case 7: /* 111 output high */
				PBCTL &= 0x3f;
				/* PACTL has: if (!PIA_CB2_negpending && PIA_CB2 == 0) */
				if (PIA_CB2 == 0) {
					PIA_CB2_pospending = 1;
				}
				set_CB2(1);
				PIA_CB2_negpending = 0;
			break;
		}
		update_PIA_IRQ();
		break;
	case _PORTA:
		if ((PACTL & 0x04) == 0) {
			/* write DDRA (data direction register A) */
 			PORTA_mask = ~byte;
		}
		else {
			/* write ORA (output register A) */
			PIA_PORTA = byte;		/* change from thor */
		}
		break;
	case _PORTB:
		if ((PBCTL & 0x04) == 0) {
			/* write DDRB (data direction register B) */
			if (myConfig.machine_type >= MACHINE_XLXE_64K) 
            {
				MEMORY_HandlePORTB((UBYTE) (PIA_PORTB | ~byte), (UBYTE) (PIA_PORTB | PORTB_mask));
			}
			PORTB_mask = ~byte;
		}
		else {
			/* write ORB (output register B) */
			if (((PBCTL & 0x38)>>3) == 0x04) { /* handshake */
				if (PIA_CB2 == 1) {
					PIA_CB2_negpending = 1;
				}
				set_CB2(0);
			}
			else if (((PBCTL & 0x38)>>3) == 0x05) { /* pulse */
				set_CB2(0);
				set_CB2(1); /* FIXME one cycle later ... */
			}
			if (myConfig.machine_type >= MACHINE_XLXE_64K) 
            {
				MEMORY_HandlePORTB((UBYTE) (byte | PORTB_mask), (UBYTE) (PIA_PORTB | PORTB_mask));
			}
			PIA_PORTB = byte;
		}
		break;
	}
}


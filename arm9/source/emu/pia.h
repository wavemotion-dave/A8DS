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

#ifndef _PIA_H_
#define _PIA_H_

#include "atari.h"

#define _PORTA 0x00
#define _PORTB 0x01
#define _PACTL 0x02
#define _PBCTL 0x03

extern UBYTE PACTL;
extern UBYTE PBCTL;
extern UBYTE PORTA;
extern UBYTE PORTB;
extern UBYTE PORTA_mask;
extern UBYTE PORTB_mask;
extern UBYTE PIA_IRQ;
extern UBYTE PORT_input[2];
extern int xe_bank;
extern int selftest_enabled;
extern UBYTE PIA_CA2;
extern UBYTE PIA_CA2_negpending;
extern UBYTE PIA_CA2_pospending;
extern UBYTE PIA_CB2;
extern UBYTE PIA_CB2_negpending;
extern UBYTE PIA_CB2_pospending;
extern UBYTE PIA_IRQ;

#define PIA_PORTA PORTA
#define PIA_PORTB PORTB

extern UBYTE atari_os[0x4000];
extern UBYTE *atari_os_pristine;

extern void PIA_Initialise(void);
extern void PIA_Reset(void);
extern UBYTE PIA_GetByte(UWORD addr);
extern void PIA_PutByte(UWORD addr, UBYTE byte);

#endif /* _PIA_H_ */

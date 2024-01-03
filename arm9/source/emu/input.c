/*
 * input.c - keyboard, joysticks and mouse emulation
 * 
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly ecommended that you seek out the latest Atari800 sources.
 *
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2024 Dave Bernazzani (wavemotion-dave)
 *
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
 *
 * Copyright (C) 2001-2002 Piotr Fusik
 * Copyright (C) 2001-2006 Atari800 development team (see DOC/CREDITS)
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

#include <string.h>
#include "antic.h"
#include "atari.h"
#include "esc.h"
#include "cartridge.h"
#include "cpu.h"
#include "gtia.h"
#include "input.h"
#include "memory.h"
#include "pia.h"
#include "platform.h"
#include "pokeysnd.h"
#include "util.h"

#define Atari_POT(x) 228
extern UBYTE PCPOT_input[8];
extern UBYTE trig0, trig1;
extern UBYTE stick0, stick1;

int key_code = AKEY_NONE;
int key_shift = 0;
int key_consol = CONSOL_NONE;

static UBYTE STICK[4], OLDSTICK[4];
static UBYTE TRIG_input[4];

void INPUT_Initialise(void) {}

void INPUT_Frame(void) 
{
    int i;
    static int last_key_code = AKEY_NONE;
    static int last_key_break = 0;
    static UBYTE last_stick[4] = {STICK_CENTRE, STICK_CENTRE, STICK_CENTRE, STICK_CENTRE};

    /* handle keyboard */
    i = (key_code == AKEY_BREAK);
  
    if (i && !last_key_break) {
        if (IRQEN & 0x80) {
            IRQST &= ~0x80;
            GenerateIRQ();
        }
    }
    last_key_break = i;

    SKSTAT |= 0xc;
    if (key_shift)
        SKSTAT &= ~8;

    if (key_code < 0) 
    {
        if (CASSETTE_press_space) 
        {
            key_code = AKEY_SPACE;
            CASSETTE_press_space = 0;
        }
        else 
        {
            last_key_code = AKEY_NONE;
        }
    }
    if (key_code >= 0) 
    {
        SKSTAT &= ~4;
        if ((key_code ^ last_key_code) & ~AKEY_SHFTCTRL) 
        {
            /* ignore if only shift or control has changed its state */
            last_key_code = key_code;
            KBCODE = (UBYTE) key_code;
            if (IRQEN & 0x40) {
                if (IRQST & 0x40) {
                    IRQST &= ~0x40;
                    GenerateIRQ();
                }
                else {
                    /* keyboard over-run */
                    SKSTAT &= ~0x40;
                    /* assert(IRQ != 0); */
                }
            }
        }
    }

    /* handle joysticks */
    i = (stick1 << 4) | stick0;
    OLDSTICK[0] = STICK[0];OLDSTICK[1] = STICK[1];
    STICK[0] = i & 0x0f;
    STICK[1] = (i >> 4) & 0x0f;

    for (i = 0; i < 2; i++) 
    {
        if ((STICK[i] & 0x0c) == 0) {   /* right and left simultaneously */
            if (last_stick[i] & 0x04)   /* if wasn't left before, move left */
                STICK[i] |= 0x08;
            else                        /* else move right */
                STICK[i] |= 0x04;
        }
        else {
            last_stick[i] &= 0x03;
            last_stick[i] |= STICK[i] & 0x0c;
        }
        if ((STICK[i] & 0x03) == 0) {   /* up and down simultaneously */
            if (last_stick[i] & 0x01)   /* if wasn't up before, move up */
                STICK[i] |= 0x02;
            else                        /* else move down */
                STICK[i] |= 0x01;
        }
        else {
            last_stick[i] &= 0x0c;
            last_stick[i] |= STICK[i] & 0x03;
        }
        
        TRIG_input[i] = (i==0 ? trig0:trig1);
        
        switch(myConfig.auto_fire)
        {
            case 1:
                if (!TRIG_input[i])  TRIG_input[i] = (gTotalAtariFrames & 16) ? 1 : 0;
                break;
            case 2:
                if (!TRIG_input[i])  TRIG_input[i] = (gTotalAtariFrames & 8) ? 1 : 0;
                break;
            case 3:
                if (!TRIG_input[i])  TRIG_input[i] = (gTotalAtariFrames & 4) ? 1 : 0;
                break;
        }
    }

    for (i = 0; i < 8; i++)
    {
        POT_input[i] = Atari_POT(i);
    }
    
    TRIG[0] = TRIG_input[0];
    TRIG[1] = TRIG_input[1];
    PORT_input[0] = (STICK[1] << 4) | STICK[0];
}


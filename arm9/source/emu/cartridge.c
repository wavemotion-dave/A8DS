/*
 * CARTRIDGE.C contains a subset of Atari800 cart support. The A8DS emulator
 * is targeting XEX and ATR files... Carts are a different beast and the only
 * "cart" we are supporting is an external Atari BASIC cart. It's very possible
 * to go back to the core Atari800 sources and pull back in all the other cart
 * type supports - but that's an exercise left to the next generation!
 *
 * A8DS - Atari 8-bit Emulator designed to run 8-bit games on the Nintendo DS/DSi
 * Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */
 
 /*
 * cartridge.c - cartridge emulation
 *
 * Copyright (C) 2001-2003 Piotr Fusik
 * Copyright (C) 2001-2005 Atari800 development team (see DOC/CREDITS)
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "a8ds.h"
#include "hash.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "memory.h"
#include "rtime.h"
#include "altirra_basic.h"

extern UBYTE ROM_basic[];
extern int basic_type;

// ------------------------------------------------------------------
// The only "cart" we support is a BASIC cart. This can either be
// Altirra built-in BASIC or an actual ATARI Rev C Basic (read 
// from SD card).
// ------------------------------------------------------------------
int CART_Insert(int enabled) 
{
    if (enabled)
    {
        // The only cart we support is an 8K built-in BASIC cart
        Cart809F_Disable();
        CartA0BF_Enable();
        SetROM(0xa000, 0xbfff);
        if (basic_type == BASIC_ALTIRRA)
        {
            CopyROM(0xa000, 0xbfff, ROM_altirra_basic);
        }
        else
        {
            CopyROM(0xa000, 0xbfff, ROM_basic);
        }
        CART_Start();
    }
    else
    {
        CART_Remove();
    }
    return 1;
}

// ------------------------------------------------------------------
// Remove the "BASIC" cart and restore the RAM under this...
// ------------------------------------------------------------------
void CART_Remove(void) 
{
    Cart809F_Disable();
    CartA0BF_Disable();
    SetRAM(0xa000, 0xbfff);
}

void CART_Start(void) 
{
    // Nothing to do... CART insert is all we need...
}

// -----------------------------------------------------------------
// A read from D500-D5FF area triggers this call... mostly we only
// need this to handle the R-Time 8 "cart" that is supported for
// date/time mostly for Sparta-DOS.
// -----------------------------------------------------------------
UBYTE CART_GetByte(UWORD addr)
{
	if (addr == 0xd5b8 || addr == 0xd5b9)
    {
		return RTIME_GetByte();
    }
    return 0;
}

// -----------------------------------------------------------------
// A write to D500-D5FF area triggrs this call... mostly we only
// need this to handle the R-Time 8 "cart" that is supported for
// date/time mostly for Sparta-DOS.
// -----------------------------------------------------------------
void CART_PutByte(UWORD addr, UBYTE byte)
{
	if (addr == 0xd5b8 || addr == 0xd5b9) 
    {
		RTIME_PutByte(byte);
	}
}


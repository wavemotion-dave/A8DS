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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "8bitutils.h"
#include "hash.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "memory.h"
#include "altirra_basic.h"

int CART_Checksum(const UBYTE *image, int nbytes)
{
    return 0;
}

extern UBYTE ROM_basic[];
extern int basic_type;
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
    }
    else
    {
        CART_Remove();
    }
    return 1;
}

void CART_Remove(void) 
{
    Cart809F_Disable();
    CartA0BF_Disable();
    SetRAM(0xa000, 0xbfff);
}

void CART_Start(void) 
{
}

UBYTE CART_GetByte(UWORD addr)
{
   (void)addr;   
    return 0;
}

void CART_PutByte(UWORD addr, UBYTE byte)
{
   (void)addr;
   (void)byte;
}


void CARTStateRead(void)
{
}

void CARTStateSave(void)
{
}



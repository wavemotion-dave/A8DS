/*
 * CARTRIDGE.C contains a subset of Atari800 cart support. The A8DS emulator
 * is targeting XEX and ATR files... We are partially supporting the use of
 * CAR (with 16-byte headers) and ROM (headerless... these are flat binary files).
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

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *strcasestr(const char *haystack, const char *needle);

#include "main.h"
#include "a8ds.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "memory.h"
#include "rtime.h"
#include "esc.h"
#include "altirra_basic.h"

extern UBYTE ROM_basic[];

UBYTE cart_image[CART_MAX_SIZE];    // Big enough to hold the largest carts we support ... 1MB
UBYTE cart_header[16];

int bank            __attribute__((section(".dtcm"))) = 0;
UBYTE cart_sic_data __attribute__((section(".dtcm"))) = 0x00;
UWORD last_bb1_bank __attribute__((section(".dtcm"))) = 1;
UWORD last_bb2_bank __attribute__((section(".dtcm"))) = 5;

UBYTE *cart_shadow = (UBYTE *)0x06880000; // Mostly for BBSB for copy of memory

/* DB_32, XEGS_32, XEGS_64, XEGS_128, XEGS_256, XEGS_512, XEGS_1024 */
/* SWXEGS_32, SWXEGS_64, SWXEGS_128, SWXEGS_256, SWXEGS_512, SWXEGS_1024 */
static void set_bank_809F(int b, int main)
{
    if (b != bank)
    {
        if (b & 0x80)
        {
            Cart809F_Disable();
            CartA0BF_Disable();
        }
        else
        {
            Cart809F_Enable();
            CartA0BF_Enable();
            mem_map[0x8] = (cart_image + b*0x2000) + 0x0000 - 0x8000;
            mem_map[0x9] = (cart_image + b*0x2000) + 0x1000 - 0x9000;

            if (bank & 0x80)
            {
                mem_map[0xA] = (cart_image + main) + 0x0000 - 0xA000;
                mem_map[0xB] = (cart_image + main) + 0x1000 - 0xB000;
            }
        }
        bank = b;
    }
}

/* OSS_16, OSS_16 */
static void set_bank_A0AF(int b, int main)
{
    if (b != bank)
    {
        if (b < 0)
            CartA0BF_Disable();
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + b*0x1000) - 0xA000;
            if (bank < 0)
            {
                mem_map[0xB] = (cart_image + main) - 0xB000;
            }
        }
        bank = b;
    }
}

/* EXP_64, DIAMOND_64, SDX_64, MIO_8, etc. */
static void set_bank_A0BF(int b)
{
    if (b != bank)
    {
        if (b & 0x0008)
        {
            CartA0BF_Disable();
        }
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + (~b&7)*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + (~b&7)*0x2000) + 0x1000 - 0xB000;
        }
        bank = b;
    }
}

/* CART_WILL_64, CART_WILL_32, CART_WILL_16 */
static void set_bank_A0BF_WILLIAMS(int b, int mask)
{
    if (b != bank) {
        if (b & 0x08)
        {
            CartA0BF_Disable();
        }
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + (b&mask)*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + (b&mask)*0x2000) + 0x1000 - 0xB000;
        }
        bank = b;
    }
}

/* TURBOSOFT_64, TURBOSOFT_128 */
static void set_bank_A0BF_TURBOSOFT(UBYTE b, UBYTE mask)
{
    if (b != bank)
    {
        if (b & 0x10)
        {
            CartA0BF_Disable();
        }
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + (b&mask)*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + (b&mask)*0x2000) + 0x1000 - 0xB000;
        }
        bank = b;
    }
}


/* CART_ATMAX_128 */
static void set_bank_A0BF_ATMAX128(int b)
{
    if (b != bank)
    {
        if (b >= 0x20)
            return;
        else if (b >= 0x10)
        {
            CartA0BF_Disable();
        }
        else {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + b*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + b*0x2000) + 0x1000 - 0xB000;
        }
        bank = b;
    }
}


/* CART_ATMAX_1024 */
void set_bank_A0BF_ATMAX1024(int b)
{
    if (b != bank)
    {
        if (b & 0x80)
        {
            CartA0BF_Disable();
        }
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + b*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + b*0x2000) + 0x1000 - 0xB000;
        }
        bank = b;
    }
}

/* CART_JATARI_8 thru CART_JATARI_1024 */
void set_bank_A0BF_JATARI(int b, int mask)
{
    if (b != bank)
    {
        if (b & 0x80)
        {
            CartA0BF_Disable();
        }
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + (b&mask)*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + (b&mask)*0x2000) + 0x1000 - 0xB000;
        }
        bank = (b&mask);
    }
}

/* CART_MEGA_16 to CART_MEGA_1024 */
static void set_bank_80BF(int b)
{
    if (b != bank)
    {
        if (b & 0x80)
        {
            Cart809F_Disable();
            CartA0BF_Disable();
        }
        else
        {
            Cart809F_Enable();
            CartA0BF_Enable();
            mem_map[0x8] = (cart_image + b*0x4000) + 0x0000 - 0x8000;
            mem_map[0x9] = (cart_image + b*0x4000) + 0x1000 - 0x9000;
            mem_map[0xA] = (cart_image + b*0x4000) + 0x2000 - 0xA000;
            mem_map[0xB] = (cart_image + b*0x4000) + 0x3000 - 0xB000;
        }
        bank = b;
    }
}

/* CART_SDX_128 */
static void set_bank_SDX_128(UWORD addr)
{
    if ((addr & 0xe0) == 0xe0 && addr != bank)
    {
        if (addr & 8)
        {
            CartA0BF_Disable();
        }
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + ((((addr & 7) + ((addr & 0x10) >> 1)) ^ 0xf)*0x2000)) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + ((((addr & 7) + ((addr & 0x10) >> 1)) ^ 0xf)*0x2000)) + 0x1000 - 0xB000;
        }
        bank = addr;
    }
}

/* CART_BLIZZARD_32 */
static void set_bank_blizzard32(UBYTE bank)
{
    if (bank < 4)
    {
        mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
    }
    else
    {
        CartA0BF_Disable();
    }
}

/* CART_ULTRACART */
static void set_bank_ultracart(UBYTE bank)
{
    if (bank < 4)
    {
        mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
    }
    else if (bank == 4)
    {
        CartA0BF_Disable();
    }
    else
    {
        bank = 0;
        mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
    }
}

/* CART_SIC_128, CART_SIC_256, CART_SIC_512 */
static void set_bank_SIC(UBYTE data, UBYTE bank_mask)
{
    UBYTE b = data & bank_mask;

    if (!(data & 0x20))
    {
        Cart809F_Disable();
    }
    else
    {
        Cart809F_Enable();
        mem_map[0x8] = cart_image + (b*0x4000) + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + (b*0x4000) + 0x1000 - 0x9000;
    }

    if (data & 0x40)
    {
        CartA0BF_Disable();
    }
    else
    {
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + (b*0x4000) + 0x2000 - 0xA000;
        mem_map[0xB] = cart_image + (b*0x4000) + 0x3000 - 0xB000;
    }

    cart_sic_data = data;
    bank = b;
}

/* CART_SIC_PLUS */
static void set_bank_SICPLUS(UBYTE data)
{
    UBYTE b = ((data & 0x80) >> 2) | (data & 0x1f);

    if (!(data & 0x20))
    {
        Cart809F_Disable();
    }
    else
    {
        Cart809F_Enable();
        mem_map[0x8] = cart_image + (b*0x4000) + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + (b*0x4000) + 0x1000 - 0x9000;
    }

    if (data & 0x40)
    {
        CartA0BF_Disable();
    }
    else
    {
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + (b*0x4000) + 0x2000 - 0xA000;
        mem_map[0xB] = cart_image + (b*0x4000) + 0x3000 - 0xB000;
    }

    cart_sic_data = data;
    bank = b;
}

/* CART_DCART */
static void set_bank_A0BF_DCART(UBYTE b)
{
    // We need to move the mini-memory window into place since the CPU emulation tends to do direct
    // memory access for emulation speed and we can't rely on the Cart_GetByte() being called always.
    if (bank != (b & 0x3f))
    {
        bank = b & 0x3f; // Even if cart becomes disabled, we need the bank number for the 'mini memory' window
        memcpy(memory+0xD500, cart_image + (bank*0x2000) + 0x1500, 256);
    }

    if (b & 0x80)    // High bit disables cart
    {
        CartA0BF_Disable();
    }
    else  // Enable 8K bank in the A000-BFFF memory range
    {
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
    }
}

/* CART_JRC_64 */
static void set_bank_A0BF_JRC(UBYTE data)
{
    bank = (data >> 4) & 0x7; // Bank is selected by the 3 bits in D6...D4
    if (data & 0x80)    // High bit disables cart
    {
        CartA0BF_Disable();
    }
    else  // Enable 8K bank in the A000-BFFF memory range
    {
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
    }
}

/* CART_JRC_64i */
static void set_bank_A0BF_JRCi(UBYTE data)
{
    static u8 jrc_interleave[] = {7,3,5,1,6,2,4,0};
    bank = (data >> 4) & 0x7; // Bank is selected by the 3 bits in D6...D4
    if (data & 0x80)    // High bit disables cart
    {
        CartA0BF_Disable();
    }
    else  // Enable 8K bank in the A000-BFFF memory range
    {
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + (jrc_interleave[bank]*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (jrc_interleave[bank]*0x2000) + 0x1000 - 0xB000;
    }
}


/* CART_ADAWLIAH32, CART_ADAWLIAH64 */
static void set_bank_ADAWLIAH(UBYTE b)
{
    bank = b;
    CartA0BF_Enable();
    mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
    mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
}

static void set_bank_xe_multicart(UBYTE b)
{
    bank = b & 0x7f;
    
    if (b & 0x80) // Is this a 16K mapping?
    {
        Cart809F_Enable(); 
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + (bank*0x2000) - 0x2000 - 0x8000;
        mem_map[0x9] = cart_image + (bank*0x2000) - 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
    }
    else // 8K mapping
    {   
        Cart809F_Disable(); 
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + (bank*0x2000) + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + (bank*0x2000) + 0x1000 - 0xB000;
    }    
}

static void set_bank_corina_1mb(UBYTE b)
{
    if (b & 0x80) // High bit disables the cart
    {
        Cart809F_Disable();
        CartA0BF_Disable();
        mem_map[0x8] = memory;
        mem_map[0x9] = memory;
        mem_map[0xA] = memory;
        mem_map[0xB] = memory;
    }
    else
    {
        if (b & 0x40) // Bit 6 enables EEPROM 8K (mirrored across 16K bank)
        {
            Cart809F_Disable();
            CartA0BF_Disable();
            mem_map[0x8] = cart_image + (1024*1024) + 0x0000 - 0x8000;
            mem_map[0x9] = cart_image + (1024*1024) + 0x1000 - 0x9000;
            mem_map[0xA] = cart_image + (1024*1024) + 0x0000 - 0xA000;
            mem_map[0xB] = cart_image + (1024*1024) + 0x1000 - 0xB000;
            SetRAM(0x8000, 0xbfff); // We treat the EE as RAM
        }
        else // ROM 1MB (banked in)
        {
            Cart809F_Enable();
            CartA0BF_Enable();
            bank = b & 0x3f;
            mem_map[0x8] = cart_image + (bank*0x4000) + 0x0000 - 0x8000;
            mem_map[0x9] = cart_image + (bank*0x4000) + 0x1000 - 0x9000;
            mem_map[0xA] = cart_image + (bank*0x4000) + 0x2000 - 0xA000;
            mem_map[0xB] = cart_image + (bank*0x4000) + 0x3000 - 0xB000;
            SetROM(0x8000, 0xbfff);
        }
    }
}

static void set_bank_corina_sram(UBYTE b)
{
    if (b & 0x80) // High bit disables the cart
    {
        Cart809F_Disable();
        CartA0BF_Disable();
        mem_map[0x8] = memory;
        mem_map[0x9] = memory;
        mem_map[0xA] = memory;
        mem_map[0xB] = memory;
    }
    else
    {
        //D5 = 0, D6 = 0 -> mapped from EPROM (ROM)
        //D5 = 1, D6 = 0 -> mapped from SRAM
        //D5 = 0, D6 = 1 -> mapped from EEPROM
        if (b & 0x40) // EEPROM
        {
            Cart809F_Disable();
            CartA0BF_Disable();
            mem_map[0x8] = cart_image + (512*1024) + 0x0000 - 0x8000;
            mem_map[0x9] = cart_image + (512*1024) + 0x1000 - 0x9000;
            mem_map[0xA] = cart_image + (512*1024) + 0x0000 - 0xA000;
            mem_map[0xB] = cart_image + (512*1024) + 0x1000 - 0xB000;
            SetRAM(0x8000, 0xbfff); // We treat the EE as RAM
        }
        else if (b & 0x20) // SRAM - we reuse the back-end empty Cart Image
        {
            Cart809F_Disable();
            CartA0BF_Disable();
            bank = b & 0x1f;
            mem_map[0x8] = cart_image + (520*1024) + (bank*0x4000) + 0x0000 - 0x8000;
            mem_map[0x9] = cart_image + (520*1024) + (bank*0x4000) + 0x1000 - 0x9000;
            mem_map[0xA] = cart_image + (520*1024) + (bank*0x4000) + 0x2000 - 0xA000;
            mem_map[0xB] = cart_image + (520*1024) + (bank*0x4000) + 0x3000 - 0xB000;
            SetRAM(0x8000, 0xbfff);
        }
        else // Normal ROM mapping area
        {
            Cart809F_Enable();
            CartA0BF_Enable();
            bank = b & 0x1f;
            mem_map[0x8] = cart_image + (bank*0x4000) + 0x0000 - 0x8000;
            mem_map[0x9] = cart_image + (bank*0x4000) + 0x1000 - 0x9000;
            mem_map[0xA] = cart_image + (bank*0x4000) + 0x2000 - 0xA000;
            mem_map[0xB] = cart_image + (bank*0x4000) + 0x3000 - 0xB000;
            SetROM(0x8000, 0xbfff);
        }
    }
}

// The cartridge with "Bounty Bob Strikes Back" game uses very strange
// bank switching method:
//  Four 4 KB banks (0,1,2,3) are mapped into $8000-$8FFF. An access to $8FF6
//   selects bank 0, $8FF7 - bank 1, $8FF8 - bank 2, $8FF9 - bank 3.
//  Four 4 KB banks (4,5,6,7) are mapped into $9000-$9FFF. An access to $9FF6
//   selects bank 4, $9FF7 - bank 5, $9FF8 - bank 6, $9FF9 - bank 7.
//  The remaining 8 KB is mapped to $A000-$BFFF.

#define CopyROM(addr1, addr2, src) memcpy(memory + (addr1), src, (addr2) - (addr1) + 1)

static void access_BountyBob1(UWORD addr)
{
    UWORD base_addr = (addr & 0xf000);
    addr &= 0x00ff;
    if (addr >= 0xf6 && addr <= 0xf9)
    {
        int new_state;
        addr -= 0xf6;

        new_state = (last_bb1_bank & 0x0c) | addr;
        if (new_state != last_bb1_bank)
        {
            CopyROM(base_addr, base_addr + 0x0fff, cart_shadow + addr * 0x1000);
            last_bb1_bank = new_state;
        }
    }
}

static void access_BountyBob2(UWORD addr)
{
    UWORD base_addr = (addr & 0xf000);
    addr &= 0x00ff;
    if (addr >= 0xf6 && addr <= 0xf9)
    {
        int new_state;
        addr -= 0xf6;
        new_state = (last_bb2_bank & 0x03) | (addr << 2);
        if (new_state != last_bb2_bank)
        {
            CopyROM(base_addr, base_addr + 0x0fff, cart_shadow + 0x4000 + addr * 0x1000);
            last_bb2_bank = new_state;
        }
    }
}

UBYTE BountyBob1GetByte(UWORD addr)
{
    access_BountyBob1(addr);
    return dGetByte(addr);
}

UBYTE BountyBob2GetByte(UWORD addr)
{
    access_BountyBob2(addr);
    return dGetByte(addr);
}

void BountyBob1PutByte(UWORD addr, UBYTE value)
{
    access_BountyBob1(addr);
}

void BountyBob2PutByte(UWORD addr, UBYTE value)
{
    access_BountyBob2(addr);
}

void BryanBank(UBYTE b)
{
    mem_map[0x4] = cart_image + ((int)b*0x8000L) + 0x0000 - 0x4000;
    mem_map[0x5] = cart_image + ((int)b*0x8000L) + 0x1000 - 0x5000;
    mem_map[0x6] = cart_image + ((int)b*0x8000L) + 0x2000 - 0x6000;
    mem_map[0x7] = cart_image + ((int)b*0x8000L) + 0x3000 - 0x7000;
    mem_map[0x8] = cart_image + ((int)b*0x8000L) + 0x4000 - 0x8000;
    mem_map[0x9] = cart_image + ((int)b*0x8000L) + 0x5000 - 0x9000;
    mem_map[0xA] = cart_image + ((int)b*0x8000L) + 0x6000 - 0xA000;
    mem_map[0xB] = cart_image + ((int)b*0x8000L) + 0x7000 - 0xB000;
}

UBYTE BryanGetByte64(UWORD addr)
{
    if (addr >= 0xBFE0) BryanBank(1);
    else if (addr >= 0xBFD0) BryanBank((addr & 0x04) ? 1:0);

    return dGetByte(addr);
}

UBYTE BryanGetByte128(UWORD addr)
{
    if (addr >= 0xBFE0) BryanBank(3);
    else if (addr >= 0xBFD0) BryanBank((addr & 0x0C) >> 2);

    return dGetByte(addr);
}


static void UnscrambleAtrax(UBYTE type, unsigned int size)
{
    /* On Atrax game and SDX cartridges, address and data lines between the
       cartridge port and the EPROM are intermixed as a kind of copy
       prevention. Data in the ROM chip, if read directly, would make no
       sense. We have to decode the ROM contents according to the connections
       on the cartridge. */

    struct cross_map_t {
        unsigned int addr[17]; /* Mapping of address (+ bank select) lines */
        unsigned char data[8]; /* Mapping of data lines */
    };
    static struct cross_map_t cross_maps[2] = {
        /* Atrax games cartridge */
        { /* cartridge port + bank select <-> EPROM */
            { 0x0020,              /*  A0 <->  A5 */
              0x0040,              /*  A1 <->  A6 */
              0x0080,              /*  A2 <->  A7 */
              0x1000,              /*  A3 <-> A12 */
              0x0001,              /*  A4 <->  A0 */
              0x0002,              /*  A5 <->  A1 */
              0x0004,              /*  A6 <->  A2 */
              0x0008,              /*  A7 <->  A3 */
              0x0010,              /*  A8 <->  A4 */
              0x0100,              /*  A9 <->  A8 */
              0x0400,              /* A10 <-> A10 */
              0x0800,              /* A11 <-> A11 */
              0x0200,              /* A12 <->  A9 */
              0x2000,              /* A13 <-> A13 */
              0x4000,              /* A14 <-> A14 */
              0x8000,              /* A15 <-> A15 */
              0x10000 },           /* A16 <-> A16 */
            { 0x10,                /*  D4 <->  Q0 */
              0x20,                /*  D5 <->  Q1 */
              0x04,                /*  D2 <->  Q2 */
              0x80,                /*  D7 <->  Q3 */
              0x08,                /*  D3 <->  Q4 */
              0x01,                /*  D0 <->  Q5 */
              0x02,                /*  D1 <->  Q6 */
              0x40 }               /*  D6 <->  Q7 */
        },
        /* Atrax SDX */
        { /* cartridge port + bank select <-> EPROM */
            { 0x0040,              /*  A0 <->  A6 */
              0x0080,              /*  A1 <->  A7 */
              0x1000,              /*  A2 <-> A12 */
              0x8000,              /*  A3 <-> A15 */
              0x4000,              /*  A4 <-> A14 */
              0x2000,              /*  A5 <-> A13 */
              0x0100,              /*  A6 <->  A8 */
              0x0020,              /*  A7 <->  A5 */
              0x0010,              /*  A8 <->  A4 */
              0x0008,              /*  A9 <->  A3 */
              0x0001,              /* A10 <->  A0 */
              0x0002,              /* A11 <->  A1 */
              0x0004,              /* A12 <->  A2 */
              0x0200,              /* A13 <->  A9 */
              0x0800,              /* A14 <-> A11 */
              0x0400,              /* A15 <-> A10 */
              0x10000 },           /* A16 <-> A16 (only on ATRAX_SDX_128) */
            { 0x02,                /*  D1 <->  Q0 */
              0x08,                /*  D3 <->  Q1 */
              0x80,                /*  D7 <->  Q2 */
              0x40,                /*  D6 <->  Q3 */
              0x01,                /*  D0 <->  Q4 */
              0x04,                /*  D2 <->  Q5 */
              0x20,                /*  D5 <->  Q6 */
              0x10 }               /*  D4 <->  Q7 */
        }
    };
    struct cross_map_t *map;

    map = &cross_maps[type];

    for (unsigned int i = 0; i < size; i++)
    {
        unsigned int const rom_addr =
            (i &  0x0001 ? map->addr[0] : 0) |
            (i &  0x0002 ? map->addr[1] : 0) |
            (i &  0x0004 ? map->addr[2] : 0) |
            (i &  0x0008 ? map->addr[3] : 0) |
            (i &  0x0010 ? map->addr[4] : 0) |
            (i &  0x0020 ? map->addr[5] : 0) |
            (i &  0x0040 ? map->addr[6] : 0) |
            (i &  0x0080 ? map->addr[7] : 0) |
            (i &  0x0100 ? map->addr[8] : 0) |
            (i &  0x0200 ? map->addr[9] : 0) |
            (i &  0x0400 ? map->addr[10] : 0) |
            (i &  0x0800 ? map->addr[11] : 0) |
            (i &  0x1000 ? map->addr[12] : 0) |
            (i &  0x2000 ? map->addr[13] : 0) |
            (i &  0x4000 ? map->addr[14] : 0) |
            (i &  0x8000 ? map->addr[15] : 0) |
            (i & 0x10000 ? map->addr[16] : 0);

        UBYTE byte = cart_image[rom_addr];

        cart_image[(512*1024) + i] =
            (byte & 0x01 ? map->data[0] : 0) |
            (byte & 0x02 ? map->data[1] : 0) |
            (byte & 0x04 ? map->data[2] : 0) |
            (byte & 0x08 ? map->data[3] : 0) |
            (byte & 0x10 ? map->data[4] : 0) |
            (byte & 0x20 ? map->data[5] : 0) |
            (byte & 0x40 ? map->data[6] : 0) |
            (byte & 0x80 ? map->data[7] : 0);
    }

    // Copy the unscrambled cart image back into place...
    memcpy(cart_image, cart_image + (512*1024), size);
}



u8 Guess5200CartType(const char *filename)
{
    if (strcasestr(filename, "Battlezone") != 0)     return CART_5200_EE_16;
    if (strcasestr(filename, "Rogers") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Centipede") != 0)      return CART_5200_EE_16;
    if (strcasestr(filename, "Bongo") != 0)          return CART_5200_EE_16;
    if (strcasestr(filename, "Countermeasure") != 0) return CART_5200_EE_16;
    if (strcasestr(filename, "Defender") != 0)       return CART_5200_EE_16;
    if (strcasestr(filename, "Dig Dug") != 0)        return CART_5200_EE_16;
    if (strcasestr(filename, "DigDug") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Frisky") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Threedeep") != 0)      return CART_5200_EE_16;
    if (strcasestr(filename, "Gyruss") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Hangly") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "James") != 0)          return CART_5200_EE_16;
    if (strcasestr(filename, "Joust") != 0)          return CART_5200_EE_16;
    if (strcasestr(filename, "Pac-Man") != 0)        return CART_5200_EE_16;
    if (strcasestr(filename, "Pac Man") != 0)        return CART_5200_EE_16;
    if (strcasestr(filename, "PacMan") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Jungle") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Kangaroo") != 0)       return CART_5200_EE_16;
    if (strcasestr(filename, "Microgammon") != 0)    return CART_5200_EE_16;
    if (strcasestr(filename, "Miniature") != 0)      return CART_5200_EE_16;
    if (strcasestr(filename, "Montezuma") != 0)      return CART_5200_EE_16;
    if (strcasestr(filename, "Popeye") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Position") != 0)       return CART_5200_EE_16;
    if (strcasestr(filename, "QIX") != 0)            return CART_5200_EE_16;
    if (strcasestr(filename, "Tennis") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Football") != 0)       return CART_5200_EE_16;
    if (strcasestr(filename, "Soccer") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Runner") != 0)         return CART_5200_EE_16;
    if (strcasestr(filename, "Dungeon") != 0)        return CART_5200_EE_16;
    if (strcasestr(filename, "Stargate") != 0)       return CART_5200_EE_16;
    if (strcasestr(filename, "Raiders") != 0)        return CART_5200_EE_16;
    if (strcasestr(filename, "Trek") != 0)           return CART_5200_EE_16;
    if (strcasestr(filename, "Wars") != 0)           return CART_5200_EE_16;
    if (strcasestr(filename, "Xari") != 0)           return CART_5200_EE_16;

    return CART_5200_NS_16; // The more common 16K
}

// ---------------------------------------------------------------------
// We support both .CAR and .ROM files and instead of copying chunks
// of memory around, we use the mem_map[] to point to various rom
// banks/segments/memory within the cartridge space. This saves us
// having to memcpy() and slowing down the emulation.
// ---------------------------------------------------------------------
int cart_size = 0;
int CART_Insert(int enabled, int file_type, const char *filename)
{
    memset(cart_image, 0xFF, sizeof(cart_image));   // Fill with 0xFF until we read in the cart
    bank = 0;

    CART_Remove();

    if (file_type == AFILE_CART)
    {
        FILE * fp = fopen(filename, "rb");
        if (fp != NULL)
        {
            fread(cart_header, 1, 16, fp);
            fread(cart_image, 1, CART_MAX_SIZE, fp);
            fclose(fp);
            myConfig.cart_type = cart_header[7];
        }
    }
    else if (file_type == AFILE_ROM)
    {
        FILE * fp = fopen(filename, "rb");
        if (fp != NULL)
        {
            cart_size = fread(cart_image, 1, CART_MAX_SIZE, fp);
            fclose(fp);
            int size = cart_size / 1024;
            // If configuration hasn't been set for a Cartridge Type, guess at the type based on ROM size
            if (myConfig.cart_type == CART_NONE)
            {
                if (size == 2)      myConfig.cart_type = CART_STD_2;
                if (size == 4)      myConfig.cart_type = CART_STD_4;
                if (size == 8)      myConfig.cart_type = CART_STD_8;
                if (size == 16)     myConfig.cart_type = CART_STD_16;
                if (size == 32)     myConfig.cart_type = CART_XEGS_32;
                if (size == 40)     myConfig.cart_type = CART_BBSB_40;
                if (size == 64)     myConfig.cart_type = CART_XEGS_64;
                if (size == 128)    myConfig.cart_type = CART_XEGS_128;
                if (size == 256)    myConfig.cart_type = CART_XEGS_256;
                if (size == 512)    myConfig.cart_type = CART_XEGS_512;
                if (size == 1024)   myConfig.cart_type = CART_ATMAX_1024;
            }
        }
    }
    else if (file_type == AFILE_A52)
    {
        FILE * fp = fopen(filename, "rb");
        if (fp != NULL)
        {
            cart_size = fread(cart_image, 1, CART_MAX_SIZE, fp);
            fclose(fp);
            int size = cart_size / 1024;
            // If configuration hasn't been set for an A52 type, guess at the type based on ROM size
            if (myConfig.cart_type == CART_NONE)
            {
                if (size == 4)      myConfig.cart_type = CART_5200_4;
                if (size == 8)      myConfig.cart_type = CART_5200_8;
                if (size == 16)     myConfig.cart_type = Guess5200CartType(filename);
                if (size == 32)     myConfig.cart_type = CART_5200_32;
                if (size == 40)     myConfig.cart_type = CART_5200_40;
                if (size == 64)     myConfig.cart_type = CART_5200_64;
                if (size == 128)    myConfig.cart_type = CART_5200_128;

                myConfig.machine_type = MACHINE_5200;
                install_os();
                MEMORY_InitialiseMachine();
            }
        }
    }

    if (enabled)
    {
        CART_Start(cart_size);
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
    if (myConfig.machine_type >= MACHINE_800_48K)
    {
        SetRAM(0x8000, 0x9fff);
        SetRAM(0xa000, 0xbfff);
        memset((UBYTE*)memory+0x8000, 0x00, 0x4000);
    }
}

// --------------------------------------------------------------
// Setup the basic memory in the 0x8000 and 0xA000 region based
// on the cartridge that has been 'inserted' into the emulation.
// --------------------------------------------------------------
void CART_Start(int cart_size)
{
    last_bb1_bank = 1;
    last_bb2_bank = 5;

    switch (myConfig.cart_type)
    {
    case CART_STD_2:
        Cart809F_Disable();
        CartA0BF_Enable();
        memcpy(cart_image+0x800, cart_image, 0x800);    // Move the 2K ROM out
        memset(cart_image, 0xFF, 0x800);                // And back-fill 0xFF
        mem_map[0xA] = cart_image + 0x1000 - 0xA000;    // There will just be 0xFF out here...
        mem_map[0xB] = cart_image - 0x0000 - 0xB000;    // The 2K is mapped at the back end
        break;
    case CART_STD_4:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x0000 - 0xB000;
        break;
    case CART_STD_8:
    case CART_PHOENIX_8:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x1000 - 0xB000;
        break;
    case CART_STD_16:
    case CART_BLIZZARD_16:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0x2000 - 0xA000;
        mem_map[0xB] = cart_image + 0x3000 - 0xB000;
        break;
    case CART_OSS_16_034M:
    case CART_OSS_16_043M:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x3000 - 0xB000;
        bank = 0;
        break;
    case CART_OSS_16:
    case CART_OSS_8:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x1000 - 0xA000;
        mem_map[0xB] = cart_image + 0x0000 - 0xB000;
        bank = 0;
        break;
    case CART_DB_32:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0x6000 - 0xA000;
        mem_map[0xB] = cart_image + 0x7000 - 0xB000;
        bank = 0;
        break;
        
    case CART_ATRAX_SDX_64:
    case CART_ATRAX_SDX_128:
        UnscrambleAtrax(1, 64*1024); // Once unscrambled, it's just SDX_64 and SDX 128
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x1000 - 0xB000;
        bank = 0;
        break;
    
    case CART_MIO_8:
       for (u16 i=1; i<8; i++) memcpy(cart_image + (i*2000), cart_image, 0x2000);   // Replicate bank 1 across 64K
    case CART_WILL_64:
    case CART_WILL_32:
    case CART_EXP_64:
    case CART_DIAMOND_64:
    case CART_SDX_64:
    case CART_SDX_128:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x1000 - 0xB000;
        bank = 0;
        break;
    case CART_XEGS_32:
    case CART_SWXEGS_32:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0x6000 - 0xA000;
        mem_map[0xB] = cart_image + 0x7000 - 0xB000;
        bank = 0;
        break;
    case CART_XEGS_64:
    case CART_SWXEGS_64:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0xe000 - 0xA000;
        mem_map[0xB] = cart_image + 0xf000 - 0xB000;
        bank = 0;
        break;
    case CART_XEGS_128:
    case CART_SWXEGS_128:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0x1e000 - 0xA000;
        mem_map[0xB] = cart_image + 0x1f000 - 0xB000;
        bank = 0;
        break;
    case CART_XEGS_256:
    case CART_SWXEGS_256:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0x3e000 - 0xA000;
        mem_map[0xB] = cart_image + 0x3f000 - 0xB000;
        bank = 0;
        break;
    case CART_XEGS_512:
    case CART_SWXEGS_512:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0x7e000 - 0xA000;
        mem_map[0xB] = cart_image + 0x7f000 - 0xB000;
        bank = 0;
        break;
    case CART_XEGS_1024:
    case CART_SWXEGS_1024:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0xfe000 - 0xA000;
        mem_map[0xB] = cart_image + 0xff000 - 0xB000;
        bank = 0;
        break;
    case CART_BBSB_40:
        Cart809F_Enable();
        CartA0BF_Enable();
        memcpy(cart_shadow, cart_image, 40*1024);
        CopyROM(0x8000, 0x8fff, cart_shadow + (last_bb1_bank & 0x03) * 0x1000);
        CopyROM(0x9000, 0x9fff, cart_shadow + 0x4000 + ((last_bb2_bank & 0x0c) >> 2) * 0x1000);
        CopyROM(0xa000, 0xbfff, cart_shadow + 0x8000);
        readmap[0x8f] = BountyBob1GetByte;
        readmap[0x9f] = BountyBob2GetByte;
        writemap[0x8f] = BountyBob1PutByte;
        writemap[0x9f] = BountyBob2PutByte;
        break;
    case CART_ATRAX_128:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x1000 - 0xB000;
        bank = 0;
        break;
    case CART_MEGA_16:
    case CART_MEGA_32:
    case CART_MEGA_64:
    case CART_MEGA_128:
    case CART_MEGA_256:
    case CART_MEGA_512:
    case CART_MEGA_1024:
        Cart809F_Enable();
        CartA0BF_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        mem_map[0xA] = cart_image + 0x2000 - 0xA000;
        mem_map[0xB] = cart_image + 0x3000 - 0xB000;
        bank = 0;
        break;
    case CART_TURBOSOFT_64:
    case CART_TURBOSOFT_128:
            bank = 99;
            set_bank_A0BF_TURBOSOFT(0x00, 0x0F);
        break;
    case CART_ATMAX_128:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x1000 - 0xB000;
        bank = 0;
        break;
    case CART_ATMAX_1024:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0xfe000 - 0xA000;
        mem_map[0xB] = cart_image + 0xff000 - 0xB000;
        bank = 0x7f;
        break;
    case CART_ATMAX_NEW_1024:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0xfe000 - 0xA000;
        mem_map[0xB] = cart_image + 0xff000 - 0xB000;
        bank = 0x00;
        break;
    case CART_JRC_64:
        Cart809F_Disable();
        set_bank_A0BF_JRC(0);
        break;
    case CART_JRC_64i:
        Cart809F_Disable();
        set_bank_A0BF_JRCi(0);
        break;
    case CART_SIC_128:
        set_bank_SIC(0x00, 0x07);
        break;
    case CART_SIC_256:
        set_bank_SIC(0x00, 0x0f);
        break;
    case CART_SIC_512:
        set_bank_SIC(0x00, 0x1f);
        break;
    case CART_SIC_PLUS:
        set_bank_SICPLUS(0x00);
        break;
    case CART_XE_MULTI_8:
    case CART_XE_MULTI_16:
    case CART_XE_MULTI_32:
    case CART_XE_MULTI_64:
    case CART_XE_MULTI_128:
    case CART_XE_MULTI_256:
    case CART_XE_MULTI_512:
    case CART_XE_MULTI_1024:
        Cart809F_Disable();
        CartA0BF_Enable();
        memcpy(memory+0xA000, cart_image, 0x2000);   // First 8K mapped in (despite cart.txt saying last)
        break;
    case CART_ULTRACART:
        Cart809F_Disable();
        CartA0BF_Enable();
        bank = 0;
        set_bank_ultracart(bank);
        break;
    case CART_LOWBANK_8:
        Cart809F_Enable();
        CartA0BF_Disable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;
        break;
    case CART_BLIZZARD_4:
        Cart809F_Disable();
        CartA0BF_Enable();
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;
        mem_map[0xB] = cart_image + 0x0000 - 0xB000;    // 4K Cart image is duplicated here
        break;
    case CART_BLIZZARD_32:
        Cart809F_Disable();
        CartA0BF_Enable();
        bank = 0;
        set_bank_blizzard32(bank);
        break;
    case CART_AST_32:
        Cart809F_Disable();
        CartA0BF_Enable();
        bank = 0;
        for (u32 i=0; i<32; i++) memcpy(cart_image+0x10000+(256*i), cart_image, 256); // Copy initial 256 byte block into unused 8K area
        mem_map[0xA] = cart_image + 0x10000 - 0xA000;        // First 256 bytes is repeated throughout the memory range
        mem_map[0xB] = cart_image + 0x11000 - 0xB000;        // First 256 bytes is repeated throughout the memory range
        memcpy(memory+0xD500, cart_image + (bank*256), 256); // First 256 bytes in the visible window
        break;
    case CART_ADAWLIAH32:
        Cart809F_Disable();
        CartA0BF_Enable();
        bank = 0;
        set_bank_ADAWLIAH(bank & 0x03);
        break;
    case CART_ADAWLIAH64:
        Cart809F_Disable();
        CartA0BF_Enable();
        bank = 0;
        set_bank_ADAWLIAH(bank & 0x07);
        break;
    case CART_JATARI_8:
    case CART_JATARI_16:
    case CART_JATARI_32:
    case CART_JATARI_64:
    case CART_JATARI_128:
    case CART_JATARI_256:
    case CART_JATARI_512:
    case CART_JATARI_1024:
        set_bank_A0BF_JATARI(0, 0x7f);
        break;
    case CART_DCART:
        Cart809F_Disable();
        set_bank_A0BF_DCART(0);     // Bank 0 on power up
        ESC_UnpatchOS();            // DCART won't work with patched OS
        myConfig.disk_speedup = 0;  // And Disk Speedup must be off
        break;
    case CART_CORINA_1MB:
        set_bank_corina_1mb(0);
        break;
    case CART_CORINA_SRAM:
        set_bank_corina_sram(0);
        break;
    case CART_5200_4:
        memset(memory+0x4000, 0xff, 0x4000);
        memcpy(memory+0x8000, cart_image, 0x1000);
        memcpy(memory+0x9000, cart_image, 0x1000);
        memcpy(memory+0xA000, cart_image, 0x1000);
        memcpy(memory+0xB000, cart_image, 0x1000);
        break;
    case CART_5200_8:
        memset(memory+0x4000, 0xff, 0x4000);
        memcpy(memory+0x8000, cart_image, 0x2000);
        memcpy(memory+0xA000, cart_image, 0x2000);
        break;
    case CART_5200_NS_16:
        memset(memory+0x4000, 0xff, 0x4000);
        memcpy(memory+0x8000, cart_image, 0x4000);
        break;
    case CART_5200_EE_16:
        memcpy(memory+0x4000, cart_image, 0x2000);
        memcpy(memory+0x6000, cart_image, 0x4000);
        memcpy(memory+0xA000, cart_image+0x2000, 0x2000);
        break;
    case CART_5200_40:
        memcpy(cart_shadow, cart_image, 40*1024);
        memcpy(memory+0x4000, cart_shadow + (last_bb1_bank & 0x03) * 0x1000, 0x1000);
        memcpy(memory+0x5000, cart_shadow + 0x4000 + ((last_bb2_bank & 0x0c) >> 2) * 0x1000, 0x1000);
        memcpy(memory+0x8000, cart_shadow+0x8000, 0x2000); // Fixed bank 8K
        memcpy(memory+0xA000, cart_shadow+0x8000, 0x2000); // Fixed bank mirror
        readmap[0x4f] = BountyBob1GetByte;
        readmap[0x5f] = BountyBob2GetByte;
        writemap[0x4f] = BountyBob1PutByte;
        writemap[0x5f] = BountyBob2PutByte;
        break;
    case CART_5200_32:
        memcpy(memory+0x4000, cart_image, 0x8000);
        break;
    case CART_5200_64:
        BryanBank(1);
        readmap[0xbf] = BryanGetByte64;
        break;
    case CART_5200_128:
        BryanBank(3);
        readmap[0xbf] = BryanGetByte128;
        break;
    case CART_TELELINK2:
        Cart809F_Disable();
        CartA0BF_Enable();
        memset(cart_image+0x2000, 0x00, 0x2000);        // Clear the 'NVRAM' area
        mem_map[0xA] = cart_image + 0x0000 - 0xA000;    // Cart is mapped here
        mem_map[0xB] = cart_image + 0x1000 - 0xB000;    // Cart is mapped here
        mem_map[0x9] = cart_image + 0x2000 - 0x9000;    // This is 'NVRAM' and we just reuse the cart space
        SetRAM(0x9000, 0x9fff); // We treat the NVRAM as RAM (not backed)
        break;
    case CART_RIGHT_4:
        memcpy(cart_image+0x1000, cart_image, 0x1000); // Duplicate 4K to fill entire region
        // No break is intentional...
    case CART_RIGHT_8:
        Cart809F_Enable();
        mem_map[0x8] = cart_image + 0x0000 - 0x8000;
        mem_map[0x9] = cart_image + 0x1000 - 0x9000;

        // With Right-side CARTs we might also be enabling BASIC
        if (myConfig.basic_enabled)
        {
            CartA0BF_Enable();
            mem_map[0xA] = ((UBYTE*)ROM_basic) + 0x0000 - 0xA000;
            mem_map[0xB] = ((UBYTE*)ROM_basic) + 0x1000 - 0xB000;
        }
        else
        {
            CartA0BF_Disable();
            memset(memory+0x8000, 0xFF, 0x2000);   // Just clear out anything in this slot...
        }
        break;

    default:
        // The only default cart we support is an 8K built-in BASIC cart
        if (myConfig.basic_enabled)
        {
            Cart809F_Disable();
            CartA0BF_Enable();
            mem_map[0xA] = ((UBYTE*)ROM_basic) + 0x0000 - 0xA000;
            mem_map[0xB] = ((UBYTE*)ROM_basic) + 0x1000 - 0xB000;
        }
        break;
    }
}

void CART_Access(UWORD addr)
{
    int b = bank;
    switch (myConfig.cart_type)
    {
    case CART_OSS_16_034M:
        if (addr & 0x08)
            b = -1;
        else
            switch (addr & 0x07)
            {
            case 0x00:
            case 0x01:
                b = 0;
                break;
            case 0x03:
            case 0x07:
                b = 1;
                break;
            case 0x04:
            case 0x05:
                b = 2;
                break;
            /* case 0x02:
            case 0x06: */
            default:
                break;
            }
        set_bank_A0AF(b, 0x3000);
        break;
    case CART_OSS_16_043M:
        if (addr & 0x08)
            b = -1;
        else
            switch (addr & 0x07)
            {
            case 0x00:
            case 0x01:
                b = 0;
                break;
            case 0x03:
            case 0x07:
                b = 2;
                break;
            case 0x04:
            case 0x05:
                b = 1;
                break;
            /* case 0x02:
            case 0x06: */
            default:
                break;
            }
        set_bank_A0AF(b, 0x3000);
        break;
    case CART_OSS_16:
        switch (addr & 0x09)
        {
        case 0x00:
            b = 1;
            break;
        case 0x01:
            b = 3;
            break;
        case 0x08:
            b = -1;
            break;
        case 0x09:
            b = 2;
            break;
        }
        set_bank_A0AF(b, 0x0000);
        break;
    case CART_OSS_8:
        switch (addr & 0x09)
        {
        case 0x00:
        case 0x01:
            b = 1;
            break;
        case 0x08:
            b = -1;
            break;
        case 0x09:
            b = 0;
            break;
        }
        set_bank_A0AF(b, 0x0000);
        break;
    case CART_DB_32:
        set_bank_809F(addr & 0x03, 0x6000);
        break;
    case CART_WILL_64:
        set_bank_A0BF_WILLIAMS(addr, 0x07);
        break;
    case CART_WILL_32:
        set_bank_A0BF_WILLIAMS(addr, 0x03);
        break;
    case CART_WILL_16:
        set_bank_A0BF_WILLIAMS(addr, 0x01);
        break;
    case CART_EXP_64:
        if ((addr & 0xf0) == 0x70)
            set_bank_A0BF(addr);
        break;
    case CART_DIAMOND_64:
        if ((addr & 0xf0) == 0xd0)
            set_bank_A0BF(addr);
        break;
    case CART_ATRAX_SDX_64:
    case CART_SDX_64:
        if ((addr & 0xf0) == 0xe0)
            set_bank_A0BF(addr);
        break;
    case CART_MIO_8:
        if ((addr & 0xf0) == 0xe0)
            set_bank_A0BF(addr);
        break;
    case CART_PHOENIX_8:
        CartA0BF_Disable();
        break;
    case CART_BLIZZARD_4:
        CartA0BF_Disable();
        break;
    case CART_BLIZZARD_16:
        Cart809F_Disable();
        CartA0BF_Disable();
        break;
    case CART_BLIZZARD_32:
        set_bank_blizzard32(++bank);
        break;
    case CART_ULTRACART:
        set_bank_ultracart(++bank);
        break;
    case CART_ATMAX_128:
        set_bank_A0BF_ATMAX128(addr & 0xff);
        break;
    case CART_ATMAX_1024:
    case CART_ATMAX_NEW_1024:
        set_bank_A0BF_ATMAX1024(addr & 0xff);
        break;
    case CART_ATRAX_SDX_128:
    case CART_SDX_128:
        set_bank_SDX_128(addr);
        break;
    case CART_TURBOSOFT_64:
        set_bank_A0BF_TURBOSOFT(addr, 0x07);
        break;
    case CART_TURBOSOFT_128:
        set_bank_A0BF_TURBOSOFT(addr, 0x0f);
        break;
    case CART_ADAWLIAH32:
        set_bank_ADAWLIAH(++bank & 0x03);
        break;
    case CART_ADAWLIAH64:
        set_bank_ADAWLIAH(++bank & 0x07);
        break;
    case CART_JATARI_8:
        set_bank_A0BF_JATARI(addr, 0x00);
        break;
    case CART_JATARI_16:
        set_bank_A0BF_JATARI(addr, 0x01);
        break;
    case CART_JATARI_32:
        set_bank_A0BF_JATARI(addr, 0x03);
        break;
    case CART_JATARI_64:
        set_bank_A0BF_JATARI(addr, 0x07);
        break;
    case CART_JATARI_128:
        set_bank_A0BF_JATARI(addr, 0x0f);
        break;
    case CART_JATARI_256:
        set_bank_A0BF_JATARI(addr, 0x1f);
        break;
    case CART_JATARI_512:
        set_bank_A0BF_JATARI(addr, 0x3f);
        break;
    case CART_JATARI_1024:
        set_bank_A0BF_JATARI(addr, 0x7f);
        break;
    case CART_DCART:
        set_bank_A0BF_DCART(addr & 0xff);
        break;
    default:    // Do nothing... this cart doesn't react to any access
        break;
    }
}

// -----------------------------------------------------------------
// A read from D500-D5FF area triggers this call... mostly we only
// need this to handle the R-Time 8 "cart" that is supported for
// date/time mostly for Sparta-DOS but a few special cart types
// also see action here...
// -----------------------------------------------------------------
UBYTE CART_GetByte(UWORD addr)
{
    if (addr == 0xd5b8 || addr == 0xd5b9)
    {
        // Disable RTIME for a few special carts
        if ((myConfig.cart_type != CART_AST_32)  &&
            (myConfig.cart_type != CART_SIC_128) &&
            (myConfig.cart_type != CART_SIC_256) &&
            (myConfig.cart_type != CART_SIC_512) &&
            (myConfig.cart_type != CART_DCART))
        {
            return RTIME_GetByte();
        }
    }

    switch (myConfig.cart_type)
    {
    case CART_SIC_128:
    case CART_SIC_256:
    case CART_SIC_512:
        if ((addr & 0xe0) == 0x00) return cart_sic_data; else return 0xFF;
        break;
    case CART_SIC_PLUS:
        if ((addr & 0xc0) == 0x00) return cart_sic_data; else return 0xFF;
        break;
    case CART_AST_32:
        return memory[addr]; // We've already moved in the 256 byte mini-window
        break;
    case CART_DCART:
        return memory[addr]; // We've already moved in the 256 byte mini-window
        break;
    default:
        CART_Access(addr);
        break;
    }

    return 0;
}

// -----------------------------------------------------------------
// A write to D500-D5FF area triggrs this call... mostly we only
// need this to handle the R-Time 8 "cart" that is supported for
// date/time mostly for Sparta-DOS and many of the other writes
// will trigger various banking...
// -----------------------------------------------------------------
void CART_PutByte(UWORD addr, UBYTE byte)
{
    if (addr == 0xd5b8 || addr == 0xd5b9)
    {
        // Disable RTIME for a few special carts
        if ((myConfig.cart_type != CART_AST_32)  &&
            (myConfig.cart_type != CART_SIC_128) &&
            (myConfig.cart_type != CART_SIC_256) &&
            (myConfig.cart_type != CART_SIC_512) &&
            (myConfig.cart_type != CART_DCART))
        {
            RTIME_PutByte(byte);
            return;
        }
    }

    switch (myConfig.cart_type)
    {
    case CART_XEGS_32:
        set_bank_809F(byte & 0x03, 0x6000);
        break;
    case CART_XEGS_64:
        set_bank_809F(byte & 0x07, 0xe000);
        break;
    case CART_XEGS_128:
        set_bank_809F(byte & 0x0f, 0x1e000);
        break;
    case CART_XEGS_256:
        set_bank_809F(byte & 0x1f, 0x3e000);
        break;
    case CART_XEGS_512:
        set_bank_809F(byte & 0x3f, 0x7e000);
        break;
    case CART_XEGS_1024:
        set_bank_809F(byte & 0x7f, 0xfe000);
        break;
    case CART_ATRAX_128:
        if (byte & 0x80) {
            if (bank >= 0) {
                CartA0BF_Disable();
                bank = -1;
            }
        }
        else {
            int b = byte & 0xf;
            if (b != bank)
            {
                CartA0BF_Enable();
                mem_map[0xA] = (cart_image + b*0x2000) + 0x0000 - 0xA000;
                mem_map[0xB] = (cart_image + b*0x2000) + 0x1000 - 0xB000;
                bank = b;
            }
        }
        break;
    case CART_MEGA_16:
        set_bank_80BF(byte & 0x80);
        break;
    case CART_MEGA_32:
        set_bank_80BF(byte & 0x81);
        break;
    case CART_MEGA_64:
        set_bank_80BF(byte & 0x83);
        break;
    case CART_MEGA_128:
        set_bank_80BF(byte & 0x87);
        break;
    case CART_MEGA_256:
        set_bank_80BF(byte & 0x8f);
        break;
    case CART_MEGA_512:
        set_bank_80BF(byte & 0x9f);
        break;
    case CART_MEGA_1024:
        set_bank_80BF(byte & 0xbf);
        break;
    case CART_SWXEGS_32:
        set_bank_809F(byte & 0x83, 0x6000);
        break;
    case CART_SWXEGS_64:
        set_bank_809F(byte & 0x87, 0xe000);
        break;
    case CART_SWXEGS_128:
        set_bank_809F(byte & 0x8f, 0x1e000);
        break;
    case CART_SWXEGS_256:
        set_bank_809F(byte & 0x9f, 0x3e000);
        break;
    case CART_SWXEGS_512:
        set_bank_809F(byte & 0xbf, 0x7e000);
        break;
    case CART_SWXEGS_1024:
        set_bank_809F(byte, 0xfe000);
        break;
    case CART_SIC_128:
        if ((addr & 0xe0) == 0x00) set_bank_SIC(byte, 0x07);
        break;
    case CART_SIC_256:
        if ((addr & 0xe0) == 0x00) set_bank_SIC(byte, 0x0f);
        break;
    case CART_SIC_512:
        if ((addr & 0xe0) == 0x00) set_bank_SIC(byte, 0x1f);
        break;
    case CART_SIC_PLUS:
        if ((addr & 0xc0) == 0x00) set_bank_SICPLUS(byte);
        break;
    case CART_BLIZZARD_4:
        CartA0BF_Disable();
        break;
    case CART_BLIZZARD_32:
        set_bank_blizzard32(++bank);
        break;
    case CART_ULTRACART:
        set_bank_ultracart(++bank);
        break;
    case CART_AST_32:
        CartA0BF_Disable();
        bank = (bank+1) & 0x7F;
        memcpy(memory+0xD500, cart_image + (bank*256), 256);
        break;
    case CART_JRC_64:
        if ((addr & 0x80) == 0) set_bank_A0BF_JRC(byte);
        break;
    case CART_JRC_64i:
        if ((addr & 0x80) == 0) set_bank_A0BF_JRCi(byte);
        break;
    case CART_CORINA_1MB:
        if ((addr & 0xff) == 0) set_bank_corina_1mb(byte);
        break;
    case CART_CORINA_SRAM:
        if ((addr & 0xff) == 0) set_bank_corina_sram(byte);
        break;
    case CART_XE_MULTI_8:
    case CART_XE_MULTI_16:
    case CART_XE_MULTI_32:
    case CART_XE_MULTI_64:
    case CART_XE_MULTI_128:
    case CART_XE_MULTI_256:
    case CART_XE_MULTI_512:
    case CART_XE_MULTI_1024:
        set_bank_xe_multicart(byte);
        break;
        
    default:
        CART_Access(addr);
        break;
    }
}


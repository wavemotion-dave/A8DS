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

#include "main.h"
#include "a8ds.h"
#include "atari.h"
#include "binload.h"
#include "cartridge.h"
#include "memory.h"
#include "rtime.h"
#include "altirra_basic.h"

extern UBYTE ROM_basic[];

UBYTE cart_image[CART_MAX_SIZE];    // Big enough to hold the largest carts we support ... 1MB
UBYTE cart_header[16];
int bank __attribute__((section(".dtcm")));
UBYTE cart_sic_data __attribute__((section(".dtcm"))) = 0x00;
UWORD last_bb1_bank __attribute__((section(".dtcm"))) = 1;
UWORD last_bb2_bank __attribute__((section(".dtcm"))) = 5;

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

/* EXP_64, DIAMOND_64, SDX_64 */
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

/* CART_WILL_64 */
static void set_bank_A0BF_WILL64(int b)
{
    if (b != bank) {
        if (b & 0x0008)
        {
            CartA0BF_Disable();
        }
        else
        {
            CartA0BF_Enable();
            mem_map[0xA] = (cart_image + (b&7)*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + (b&7)*0x2000) + 0x1000 - 0xB000;
        }
        bank = b;
    }
}

/* TURBOSOFT_64, TURBOSOFT_128 */
static void set_bank_A0BF_TURBOSOFT(UBYTE b, UBYTE mask)
{
    if (b != bank)
    {
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


/* CART_WILL_32 */
static void set_bank_A0BF_WILL32(int b)
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
            mem_map[0xA] = (cart_image + (b&3)*0x2000) + 0x0000 - 0xA000;
            mem_map[0xB] = (cart_image + (b&3)*0x2000) + 0x1000 - 0xB000;
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

/* CART_DCART */
static void set_bank_A0BF_DCART(UBYTE b)
{
    // We need to move the mini-memory window into place since the CPU emulation tends to do direct 
    // memory access for emulation speed and we can't rely on the Cart_GetByte() being called always.
    bank = b & 0x3f; // Even if cart becomes disabled, we need the bank number for the 'mini memory' window
    memcpy(memory+0xD500, cart_image + (bank*0x2000) + 0x1500, 256);

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
            CopyROM(base_addr, base_addr + 0x0fff, cart_image + addr * 0x1000);
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
            CopyROM(base_addr, base_addr + 0x0fff, cart_image + 0x4000 + addr * 0x1000);
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


// ---------------------------------------------------------------------
// We support both .CAR and .ROM files and instead of copying chunks
// of memory around, we use the mem_map[] to point to various rom 
// banks/segments/memory within the cartridge space. This saves us 
// having to memcpy() and slowing down the emulation.
// ---------------------------------------------------------------------
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
    if (file_type == AFILE_ROM)
    {
        FILE * fp = fopen(filename, "rb");
        if (fp != NULL)
        {
            int size = fread(cart_image, 1, CART_MAX_SIZE, fp);
            fclose(fp);
            size = size / 1024;
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

    if (enabled)
    {
        CART_Start();
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
    SetRAM(0x8000, 0x9fff);
    SetRAM(0xa000, 0xbfff);
    memset((UBYTE*)memory+0x8000, 0xFF, 0x4000);
}

// --------------------------------------------------------------
// Setup the basic memory in the 0x8000 and 0xA000 region based
// on the cartridge that has been 'inserted' into the emulation.
// --------------------------------------------------------------
void CART_Start(void)
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
        CopyROM(0x8000, 0x8fff, cart_image + (last_bb1_bank & 0x03) * 0x1000);
        CopyROM(0x9000, 0x9fff, cart_image + 0x4000 + ((last_bb2_bank & 0x0c) >> 2) * 0x1000);
        CopyROM(0xa000, 0xbfff, cart_image + 0x8000);
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
    case CART_RIGHT_8:
        Cart809F_Disable();
        CartA0BF_Disable();
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
    case CART_SIC_128:
        set_bank_SIC(0x00, 0x07);
        break;
    case CART_SIC_256:
        set_bank_SIC(0x00, 0x0f);
        break;
    case CART_SIC_512:
        set_bank_SIC(0x00, 0x1f);
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
        mem_map[0xA] = cart_image + 0x10000 - 0xA000;   // First 256 bytes is repeated throughout the memory range
        mem_map[0xB] = cart_image + 0x11000 - 0xB000;   // First 256 bytes is repeated throughout the memory range
        break;
    case CART_DCART:
        Cart809F_Disable();
        set_bank_A0BF_DCART(0); // Bank 0 on power up
        break;

    default:
        // The only default cart we support is an 8K built-in BASIC cart
        if (myConfig.basic_type)
        {
            Cart809F_Disable();
            CartA0BF_Enable();
            SetROM(0xa000, 0xbfff);
            if (myConfig.basic_type == BASIC_ALTIRRA)
            {
                mem_map[0xA] = ((UBYTE*)ROM_altirra_basic) + 0x0000 - 0xA000;
                mem_map[0xB] = ((UBYTE*)ROM_altirra_basic) + 0x1000 - 0xB000;
            }
            else
            {
                mem_map[0xA] = ((UBYTE*)ROM_basic) + 0x0000 - 0xA000;
                mem_map[0xB] = ((UBYTE*)ROM_basic) + 0x1000 - 0xB000;
            }
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
        set_bank_A0BF_WILL64(addr);
        break;
    case CART_WILL_32:
        set_bank_A0BF_WILL32(addr);
        break;
    case CART_EXP_64:
        if ((addr & 0xf0) == 0x70)
            set_bank_A0BF(addr);
        break;
    case CART_DIAMOND_64:
        if ((addr & 0xf0) == 0xd0)
            set_bank_A0BF(addr);
        break;
    case CART_SDX_64:
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
    case CART_SDX_128:
        set_bank_SDX_128(addr);
        break;
    case CART_TURBOSOFT_64:
        set_bank_A0BF_TURBOSOFT(addr, 0x07);
        break;
    case CART_TURBOSOFT_128:
        set_bank_A0BF_TURBOSOFT(addr, 0x0f);
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
    case CART_AST_32:
        return cart_image[(256 * bank) + (addr&0xff)];
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
        break;

    default:
        CART_Access(addr);
        break;
    }
}


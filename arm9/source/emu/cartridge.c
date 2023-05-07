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

#include <nds.h>
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
extern int bHaveBASIC;

UBYTE cart_image[CART_MAX_SIZE];
UBYTE cart_header[16];
static int bank __attribute__((section(".dtcm")));
static int last_main __attribute__((section(".dtcm"))) = 33333;


// Swap in bank using 32-bit moves... 
ITCM_CODE void BankSwap(UBYTE *from, UBYTE *to, UWORD size)
{
    u32 *src = (u32*)from;
    u32 *dest = (u32*)to;
    for (int i=0; i < (size>>3); i++)
    {
        *dest++ = *src++;*dest++ = *src++;
    }
}


 void __attribute__((noinline))  SwapMainBank(int main)
{
    if (last_main != main)
    {
        CopyROM(0xa000, 0xbfff, cart_image + main);
        last_main = main;
    }
}

/* DB_32, XEGS_32, XEGS_64, XEGS_128, XEGS_256, XEGS_512, XEGS_1024 */
/* SWXEGS_32, SWXEGS_64, SWXEGS_128, SWXEGS_256, SWXEGS_512, SWXEGS_1024 */
ITCM_CODE static void set_bank_809F(int b, int main)
{
    if (b != bank) {
        if (b & 0x80) {
            Cart809F_Disable();
            CartA0BF_Disable();
        }
        else {
            Cart809F_Enable();
            CartA0BF_Enable();
            
            BankSwap((b<34) ? ((u8*)0x06860000+b*0x2000) : (b<50) ? ((u8*)0x06820000+(b-34)*0x2000) : (cart_image + b*0x2000), memory+0x8000, 0x2000);
            
            if (bank & 0x80)
            {
                SwapMainBank(main);
            }
        }
        bank = b;
    }
}

/* OSS_16, OSS2_16 */
static void set_bank_A0AF(int b, int main)
{
    if (b != bank) {
        if (b < 0)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xafff, cart_image + b * 0x1000);
            if (bank < 0)
                CopyROM(0xb000, 0xbfff, cart_image + main);
        }
        bank = b;
    }
}

/* EXP_64, DIAMOND_64, SDX_64 */
static void set_bank_A0BF(int b)
{
    if (b != bank) {
        if (b & 0x0008)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            BankSwap(((u8*)0x06860000+(~b&7)*0x2000), memory+0xa000, 0x2000);
        }
        bank = b;
    }
}

/* CART_WILL_64 */
static void set_bank_A0BF_WILL64(int b)
{
    if (b != bank) {
        if (b & 0x0008)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            BankSwap(((u8*)0x06860000+(b&7)*0x2000), memory+0xa000, 0x2000);
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
            CartA0BF_Disable();
        else 
        {
            CartA0BF_Enable();
            BankSwap(((u8*)0x06860000+(b&3)*0x2000), memory+0xa000, 0x2000);
        }
        bank = b;
    }
}

/* CART_ATMAX_128 */
static void set_bank_A0BF_ATMAX128(int b)
{
    if (b != bank) {
        if (b >= 0x20)
            return;
        else if (b >= 0x10)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            BankSwap(((u8*)0x06860000+b*0x2000), memory+0xa000, 0x2000);
        }
        bank = b;
    }
}


/* CART_ATMAX_1024 */
ITCM_CODE void set_bank_A0BF_ATMAX1024(int b)
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
            BankSwap((b<34) ? ((u8*)0x06860000+b*0x2000) : (b<50) ? ((u8*)0x06820000+(b-34)*0x2000) : (cart_image + b*0x2000), memory+0xa000, 0x2000);
        }
        bank = b;
    }
}

/* CART_MEGA_16 to CART_MEGA_1024 */
static void set_bank_80BF(int b)
{
    if (b != bank) {
        if (b & 0x80) {
            Cart809F_Disable();
            CartA0BF_Disable();
        }
        else {
            Cart809F_Enable();
            CartA0BF_Enable();
            if (b < 17)      BankSwap(((u8*)0x06860000+b*0x4000), memory+0x8000, 0x4000);
            else if (b < 25) BankSwap(((u8*)0x06820000+(b-34)*0x4000), memory+0x8000, 0x4000);
            else             BankSwap((cart_image + b*0x4000), memory+0x8000, 0x4000);
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
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image + (((addr & 7) + ((addr & 0x10) >> 1)) ^ 0xf) * 0x2000);
        }
        bank = addr;
    }
}

// ---------------------------------------------------------------------
// We support both .CAR and .ROM files but the bankswapping on those
// is really CPU intensive since we need to move chunks of memory to
// and from the main memory[] RAM area. We can't easily pull the adjust
// pointers trick since each banking scheme is a bit different and 
// testing for that would make almost game slowdown and that's not
// acceptable for this portable A8 emulator. So we do the best we can...
// some games will run fine and some will be a little slow depending on
// how much banking is going as the game runs.
// ---------------------------------------------------------------------
int CART_Insert(int enabled, int file_type, const char *filename) 
{
    memset(cart_image, 0x00, sizeof(cart_image));
    bank = 0;
    last_main=33333;
    cart_type = CART_NONE;
    CART_Remove();
    
    if (file_type == AFILE_CART)
    {
        FILE * fp = fopen(filename, "rb");
        if (fp != NULL)
        {
            fread(cart_header, 1, 16, fp);
            fread(cart_image, 1, CART_MAX_SIZE, fp);
            fclose(fp);
            memcpy((u8*)0x06860000, cart_image, 272*1024);
            memcpy((u8*)0x06820000, cart_image+(272*1024), 128*1024);
            cart_type = cart_header[7];
        }
    }
    if (file_type == AFILE_ROM)
    {
        FILE * fp = fopen(filename, "rb");
        if (fp != NULL)
        {
            int size = fread(cart_image, 1, CART_MAX_SIZE, fp);
            fclose(fp);
            memcpy((u8*)0x06860000, cart_image, 272*1024);
            size = size / 1024;
            if (size == 8)  cart_type = CART_STD_8;
            if (size == 16) cart_type = CART_STD_16;
            if (size == 32) cart_type = CART_XEGS_32;
            if (size == 64) cart_type = CART_XEGS_64;
            if (size == 128) cart_type = CART_XEGS_128;
            if (size == 256) cart_type = CART_XEGS_256;
            if (size == 512) cart_type = CART_XEGS_512;
            if (size == 1024) cart_type = CART_ATMAX_1024;
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

void CART_Start(void) 
{
    switch (cart_type) 
    {
    case CART_STD_8:
    case CART_PHOENIX_8:
        Cart809F_Disable();
        CartA0BF_Enable();
        CopyROM(0xa000, 0xbfff, cart_image);
        break;
    case CART_STD_16:
    case CART_BLIZZARD_16:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0xbfff, cart_image);
        break;
    case CART_OSS_16:
        Cart809F_Disable();
        CartA0BF_Enable();
        CopyROM(0xa000, 0xafff, cart_image);
        CopyROM(0xb000, 0xbfff, cart_image + 0x3000);
        bank = 0;
        break;
    case CART_DB_32:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0x9fff, cart_image);
        CopyROM(0xa000, 0xbfff, cart_image + 0x6000);
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
        CopyROM(0xa000, 0xbfff, cart_image);
        bank = 0;
        break;
    case CART_XEGS_32:
    case CART_SWXEGS_32:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0x9fff, cart_image);
        CopyROM(0xa000, 0xbfff, cart_image + 0x6000);
        bank = 0;
        break;
    case CART_XEGS_64:
    case CART_SWXEGS_64:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0x9fff, cart_image);
        CopyROM(0xa000, 0xbfff, cart_image + 0xe000);
        bank = 0;
        break;
    case CART_XEGS_128:
    case CART_SWXEGS_128:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0x9fff, cart_image);
        CopyROM(0xa000, 0xbfff, cart_image + 0x1e000);
        bank = 0;
        break;
    case CART_XEGS_256:
    case CART_SWXEGS_256:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0x9fff, cart_image);
        CopyROM(0xa000, 0xbfff, cart_image + 0x3e000);
        bank = 0;
        break;
    case CART_XEGS_512:
    case CART_SWXEGS_512:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0x9fff, cart_image);
        CopyROM(0xa000, 0xbfff, cart_image + 0x7e000);
        bank = 0;
        break;
    case CART_XEGS_1024:
    case CART_SWXEGS_1024:
        Cart809F_Enable();
        CartA0BF_Enable();
        CopyROM(0x8000, 0x9fff, cart_image);
        CopyROM(0xa000, 0xbfff, cart_image + 0xfe000);
        bank = 0;
        break;
    case CART_OSS2_16:
        Cart809F_Disable();
        CartA0BF_Enable();
        CopyROM(0xa000, 0xafff, cart_image + 0x1000);
        CopyROM(0xb000, 0xbfff, cart_image);
        bank = 0;
        break;
    case CART_ATRAX_128:
        Cart809F_Disable();
        CartA0BF_Enable();
        CopyROM(0xa000, 0xbfff, cart_image);
        bank = 0;
        break;
    case CART_BBSB_40:
        Cart809F_Disable();
        CartA0BF_Disable();
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
        CopyROM(0x8000, 0xbfff, cart_image);
        bank = 0;
        break;
    case CART_ATMAX_128:
        Cart809F_Disable();
        CartA0BF_Enable();
        CopyROM(0xa000, 0xbfff, cart_image);
        bank = 0;
        break;
    case CART_ATMAX_1024:
        Cart809F_Disable();
        CartA0BF_Enable();
        CopyROM(0xa000, 0xbfff, cart_image + 0xfe000);
        bank = 0x7f;
        if (skip_frames == 0) skip_frames=1; // It's the only way this will have enough speed due to the massive bankswaps
        break;
    default:
        // The only cart we support is an 8K built-in BASIC cart
        if (bHaveBASIC)
        {
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
        break;
    }
}

void CART_Access(UWORD addr)
{
    int b = bank;
    switch (cart_type) 
    {
    case CART_OSS_16:
        if (addr & 0x08)
            b = -1;
        else
            switch (addr & 0x07) {
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
    case CART_OSS2_16:
        switch (addr & 0x09) {
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
    case CART_PHOENIX_8:
        CartA0BF_Disable();
        break;
    case CART_BLIZZARD_16:
        Cart809F_Disable();
        CartA0BF_Disable();
        break;
    case CART_ATMAX_128:
        set_bank_A0BF_ATMAX128(addr & 0xff);
        break;
    case CART_ATMAX_1024:
        set_bank_A0BF_ATMAX1024(addr & 0xff);
        break;
	case CART_SDX_128:
		set_bank_SDX_128(addr);
		break;            
    default:
        break;
    }
}

// -----------------------------------------------------------------
// A read from D500-D5FF area triggers this call... mostly we only
// need this to handle the R-Time 8 "cart" that is supported for
// date/time mostly for Sparta-DOS.
// -----------------------------------------------------------------
UBYTE CART_GetByte(UWORD addr)
{
    if (cart_type != CART_NONE)
    {
        if (addr == 0xd5b8 || addr == 0xd5b9)
        {
            return RTIME_GetByte();
        }
        CART_Access(addr);
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
        return;
	}
    
    switch (cart_type) 
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
            if (b != bank) {
                CartA0BF_Enable();
                CopyROM(0xa000, 0xbfff, cart_image + b * 0x2000);
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
    default:
        CART_Access(addr);
        break;
    }
}


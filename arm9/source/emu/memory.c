/*
 * MEMORY.C contains the emulation of the RAM Memory system for the Atari 800/XL/XE
 * 
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly recommended you seek out the latest Atari800 sources.
 * 
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)

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
 * memory.c - memory emulation
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

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "antic.h"
#include "cpu.h"
#include "esc.h"
#include "cartridge.h"
#include "gtia.h"
#include "memory.h"
#include "pia.h"
#include "pokeysnd.h"
#include "util.h"

UBYTE memory[65536 + 2] __attribute__ ((aligned (4)));                  // This is the main Atari 8-bit memory which is 64K in length plus a small buffer for safety
UBYTE under_atarixl_os[16384] __attribute__ ((aligned (4)));            // This is the 16K of OS memory that co-insides with some of the RAM in the upper bank

UBYTE fast_page[0x1000] __attribute__((section(".dtcm")));              // Fast memory for the first 4K of main memory

rdfunc readmap[256] __attribute__((section(".dtcm")));                  // The readmap tells the memory fetcher if we should do direct memory read or call a device function instead
wrfunc writemap[256] __attribute__((section(".dtcm")));                 // The writemap tells the memory fetcher if we should do direct memory read or call a device function instead
UBYTE *atarixe_memory __attribute__((section(".dtcm"))) = NULL;         // Pointer to XE memory (expanded RAM)

int cart809F_enabled __attribute__((section(".dtcm"))) = FALSE;         // By default, no CART memory mapped to 0x8000 - 0x9FFF
int cartA0BF_enabled __attribute__((section(".dtcm"))) = FALSE;         // By default, no CART memory mapped to 0xA000 - 0xBFFF
UBYTE *mem_map[20] __attribute__((section(".dtcm")));                   // This is the magic that allows us to index into banks of memory quickly

// ------------------------------------------------------------------
// This is the huge 1MB+ buffer to support the maximum expanded RAM 
// for the emulator. We support the 1088K version of the Atari XL/XE
// which only a few games can even access...  The DS has 4MB of 
// general RAM available and that must hold all our data plus the 
// XEGS-DS.NDS executable itself. So this takes up a full 25% of our
// available RAM. Stil... there isn't much else to do with the NDS
// RAM so we may as well get the most out of it!  
// ------------------------------------------------------------------
UBYTE xe_mem_buffer[1024 * 1024]; // Expanded banks are 1..64 but we subtract one when indexing to give us 0..63 and 1024K of expanded memory (+64K base = 1088K)

void ROM_PutByte(UWORD addr, UBYTE value) {}

// -----------------------------------------------------------
// Force 48k and remove any XE memory buffers...
// This is stock Atari 800 for base compatibility of games...
// -----------------------------------------------------------
static void SetAtari800Memory(void)
{
    ram_size = RAM_48K; // Force 48k... 
    atarixe_memory = NULL;
}
// ------------------------------------------------------------
// XL/XE has three supported memories... 128K, 320K and 1088K
// ------------------------------------------------------------
static void SetAtariXLXEMemory(void)
{
    if (myConfig.ram_type == 1) ram_size = RAM_320_RAMBO; 
    else if (myConfig.ram_type == 2) ram_size = RAM_1088K; 
    else if (myConfig.ram_type == 3) ram_size = RAM_576_COMPY; 
    else ram_size = RAM_128K;
}

// ----------------------------------------------------------------------------------------------
// Note: We support 3 memory configurations for XE... Standard 130XE compatible 128K and
// the RAMBO 320K, COMPY 576K or RAMBO 1088K.  There is also a backwards compatible 48K option.
// ----------------------------------------------------------------------------------------------
static void AllocXEMemory(void)
{
    if (ram_size > 64) 
    {
        /* don't count 64 KB of base memory */
        /* count number of 16 KB banks, add 1 for saving base memory 0x4000-0x7fff */
        ULONG size = (1 + (ram_size - 64) / 16) * 16384;
        atarixe_memory = (UBYTE *) xe_mem_buffer; // Use the large 1088K buffer even if we don't use it all
        memset(atarixe_memory, 0x00, size);
    }
    /* atarixe_memory not needed, free it */
    else if (atarixe_memory != NULL) 
    {
        atarixe_memory = NULL;
    }
}

// ---------------------------------------------------------------------------------------
// We don't support any of the Paralell Bus interface stuff... not needed for any gaming!
// ---------------------------------------------------------------------------------------
void PBI_Initialise(void) {}
UBYTE PBI_GetByte(UWORD addr) {return 0;}
void PBI_PutByte(UWORD addr, UBYTE byte) {}
UBYTE PBIM1_GetByte(UWORD addr) {return 0;}
void PBIM1_PutByte(UWORD addr, UBYTE byte) {}
UBYTE PBIM2_GetByte(UWORD addr) {return 0;}
void PBIM2_PutByte(UWORD addr, UBYTE byte) {}

// ---------------------------------------------------------------------------------
// We call this on every cold start - it sets up the OS and the memory map for 
// the given machine type. It also removes any "carts" that might be mapped into
// the 0x8000 to 0xBFFF area (we can re-map BASIC into this region if needed).
// ---------------------------------------------------------------------------------
void MEMORY_InitialiseMachine(void) 
{
    // Start with all memory clear...
    memset(memory, 0x00, sizeof(memory));
    memset(xe_mem_buffer, 0x00, sizeof(xe_mem_buffer));
    
    // Set the memory map back to pointing to main memory
    for (int i=0; i<16; i++)
    {
        mem_map[i] = memory + (0x1000 * i) - (0x1000 * i);  // Yes, pointless except to get across the point that we are offsetting the memory map to avoid having to mask the addr in memory.h
    }
    
    // We have 4K of fast DTCM memory that we will map to a common 4K RAM space on the Atari
    mem_map[0x02]=fast_page-0x2000;
    
    mem_map[UNDER_0x8] = mem_map[0x8];
    mem_map[UNDER_0x9] = mem_map[0x9];
    mem_map[UNDER_0xA] = mem_map[0xA];
    mem_map[UNDER_0xB] = mem_map[0xB];

    switch (machine_type) 
    {
    case MACHINE_OSA:
    case MACHINE_OSB:
        SetAtari800Memory();
            
        memcpy(memory + 0xd800, atari_os, 0x2800);
        ESC_PatchOS();
        dFillMem(0x0000, 0x00, ram_size * 1024 - 1);
        SetRAM(0x0000, ram_size * 1024 - 1);
        if (ram_size < 52) 
        {
            dFillMem(ram_size * 1024, 0xff, 0xd000 - ram_size * 1024);
            SetROM(ram_size * 1024, 0xcfff);
        }
        readmap[0xd0] = GTIA_GetByte;
        readmap[0xd1] = PBI_GetByte;
        readmap[0xd2] = POKEY_GetByte;
        readmap[0xd3] = PIA_GetByte;
        readmap[0xd4] = ANTIC_GetByte;
        readmap[0xd5] = CART_GetByte;
        readmap[0xd6] = PBIM1_GetByte;
        readmap[0xd7] = PBIM2_GetByte;
        writemap[0xd0] = GTIA_PutByte;
        writemap[0xd1] = PBI_PutByte;
        writemap[0xd2] = POKEY_PutByte;
        writemap[0xd3] = PIA_PutByte;
        writemap[0xd4] = ANTIC_PutByte;
        writemap[0xd5] = CART_PutByte;
        writemap[0xd6] = PBIM1_PutByte;
        writemap[0xd7] = PBIM2_PutByte;
        SetROM(0xd800, 0xffff);
        break;
            
    case MACHINE_XLXE:
        SetAtariXLXEMemory();    
            
        int os_size = 0x4000;
        int os_rom_start = 0x10000 - os_size;
        int base_ram = ram_size > 64 ? 64 * 1024 : ram_size * 1024;
        int hole_end = (os_rom_start < 0xd000 ? os_rom_start : 0xd000);
        int hole_start = base_ram > hole_end ? hole_end : base_ram;
        antic_xe_ptr = NULL;
            
        memcpy(memory + 0xc000, atari_os, 0x4000);
        ESC_PatchOS();
            
        dFillMem(0x0000, 0x00, hole_start);
        SetRAM(0x0000, hole_start - 1);
        if (hole_start < hole_end) 
        {
            dFillMem(hole_start, 0xff, hole_end - hole_start);
            SetROM(hole_start, hole_end - 1);
        }
        if (hole_end < 0xd000)
            SetROM(hole_end, 0xcfff);

        readmap[0xd0] = GTIA_GetByte;
        readmap[0xd1] = PBI_GetByte;
        readmap[0xd2] = POKEY_GetByte;
        readmap[0xd3] = PIA_GetByte;
        readmap[0xd4] = ANTIC_GetByte;
        readmap[0xd5] = CART_GetByte;
        readmap[0xd6] = PBIM1_GetByte;
        readmap[0xd7] = PBIM2_GetByte;
        writemap[0xd0] = GTIA_PutByte;
        writemap[0xd1] = PBI_PutByte;
        writemap[0xd2] = POKEY_PutByte;
        writemap[0xd3] = PIA_PutByte;
        writemap[0xd4] = ANTIC_PutByte;
        writemap[0xd5] = CART_PutByte;
        writemap[0xd6] = PBIM1_PutByte;
        writemap[0xd7] = PBIM2_PutByte;
        SetROM(0xd800, 0xffff);
        break;
    }
    AllocXEMemory();
    
    Cart809F_Disable();    
    CartA0BF_Disable();    
    
    Coldstart();
}

// -----------------------------------------------------------------------
// Returns non-zero, if Atari BASIC is disabled by given PORTB output.
// BASIC is disabled by setting bit 1 in PortB and is also disabled
// if our RAM size is larger than 576K
// -----------------------------------------------------------------------
static int basic_disabled(UBYTE portb)
{
    return (portb & 0x02) != 0
     || ((portb & 0x10) == 0 && (ram_size == 576 || ram_size == 1088));
}


// --------------------------------------------------------------------------
// Note: this function is only for XL/XE and handles writing of Port B for
// bank switching. Only RAM sizes above 64k are useful for this routine...
//
// Although this was originally taken from the Atari800 emulator source code
// it has been heavily modified so that we don't try to swap/move 16K of RAM
// in and out of the 0x4000 to 0x7FFF region but instead we keep a pointer
// to the expanded RAM area and by using this pointer in the dGetByte() and
// dPutByte() routines (see memory.h) we can make accessing expanded RAM
// an order of magnitude faster than it has been in the past. This is really
// needed speed on the older DS hardware that struggles to move 16K of RAM
// around up to 500x per second. With this new scheme, awesome games like
// PANG, Commando320, BombJack, Bosconian and AtariBlast! are playable!
// --------------------------------------------------------------------------
void MEMORY_HandlePORTB(UBYTE byte, UBYTE oldval)
{
    /* Switch XE memory bank in 0x4000-0x7fff */
    if (ram_size > 64) 
    {
        int bank = 0;
        /* bank = 0 : base RAM */
        /* bank = 1..64 : extended RAM */
        if ((byte & 0x10) == 0)
        {
            if (ram_size == RAM_128K)
                bank = ((byte & 0x0c) >> 2) + 1;
            else if (ram_size == RAM_320_RAMBO)
                bank = (((byte & 0x0c) + ((byte & 0x60) >> 1)) >> 2) + 1;
            else if (ram_size == RAM_576_COMPY)
                bank = (((byte & 0x0e) + ((byte & 0x60) >> 1)) >> 1) + 1;
            else // Assume RAM_1088K
                bank = (((byte & 0x0e) + ((byte & 0xe0) >> 1)) >> 1) + 1;
        }
        /* Note: in Compy Shop bit 5 (ANTIC access) disables Self Test */
        if (selftest_enabled && (bank != xe_bank)) 
        {
            /* Disable Self Test ROM */
            memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
            SetRAM(0x5000, 0x57ff);
            selftest_enabled = FALSE;
        }
        
        // --------------------------------------------------------------------------------
        // This is the bank switching area for memory > 64k... it's basically a 16k
        // bank that is always swapped in/out from memory address 0x4000 to 0x7FFF
        // To test this range: (addr & 0xC000 == 0x4000) <== middle "bankswap" bank
        // --------------------------------------------------------------------------------
        if (bank != xe_bank) 
        {
            UBYTE *memory_bank;
            if (bank == 0)
            {
                memory_bank = memory;
            }
            else
            {
                memory_bank = (atarixe_memory + ((bank-1) << 14));
                memory_bank -= 0x4000;
            }
            // Apply no offsets here so we can avoid having to mask addr in memory.h
            mem_map[0x4] = memory_bank;
            mem_map[0x5] = memory_bank;
            mem_map[0x6] = memory_bank;
            mem_map[0x7] = memory_bank;
            
            xe_bank = bank;
        }
        
        // -------------------------------------------------------
        // The 128k XE RAM and the COMPY 576k RAM allow the Antic 
        // to index into the RAM independently ... tricky stuff!
        // -------------------------------------------------------
        if ((ram_size == RAM_128K) || (ram_size == RAM_576_COMPY))
        {
            switch (byte & 0x30)
            {
            case 0x20:  /* ANTIC: base, CPU: extended */
                antic_xe_ptr = memory + 0x4000;
                break;
            case 0x10:  /* ANTIC: extended, CPU: base */
                if (ram_size == RAM_128K)
                    antic_xe_ptr = atarixe_memory + ((((byte & 0x0c) >> 2)) << 14);
                else // Assume 576K COMPY
                    antic_xe_ptr = atarixe_memory + ((((byte & 0x0e) + ((byte & 0x60) >> 1)) >> 1) << 14);
                break;
            default:    /* ANTIC same as CPU */
                antic_xe_ptr = NULL;
                break;
            }
        }
    }

    /* Enable/disable OS ROM in 0xc000-0xcfff and 0xd800-0xffff */
    if ((oldval ^ byte) & 0x01) {
        if (byte & 0x01) {
            /* Enable OS ROM */
            if (ram_size > 48) {
                memcpy(under_atarixl_os, memory + 0xc000, 0x1000);
                memcpy(under_atarixl_os + 0x1800, memory + 0xd800, 0x2800);
                SetROM(0xc000, 0xcfff);
                SetROM(0xd800, 0xffff);
            }
            memcpy(memory + 0xc000, atari_os, 0x1000);
            memcpy(memory + 0xd800, atari_os + 0x1800, 0x2800);
            ESC_PatchOS();
        }
        else 
        {
            /* Disable OS ROM */
            if (ram_size > 48) 
            {
                memcpy(memory + 0xc000, under_atarixl_os, 0x1000);
                memcpy(memory + 0xd800, under_atarixl_os + 0x1800, 0x2800);
                SetRAM(0xc000, 0xcfff);
                SetRAM(0xd800, 0xffff);
            } 
            else 
            {
                dFillMem(0xc000, 0xff, 0x1000);
                dFillMem(0xd800, 0xff, 0x2800);
            }
            /* When OS ROM is disabled we also have to disable Self Test - Jindroush */
            if (selftest_enabled) 
            {
                memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
                SetRAM(0x5000, 0x57ff);
            }
            selftest_enabled = FALSE;
        }
    }

    /* Enable/disable BASIC ROM in 0xa000-0xbfff */
    if (!cartA0BF_enabled) 
    {
        /* BASIC is disabled if bit 1 set or accessing extended 576K or 1088K memory */
        int now_disabled = basic_disabled(byte);
        if (basic_disabled(oldval) != now_disabled) 
        {
            if (now_disabled) 
            {
                /* Disable BASIC ROM */
                mem_map[0xA] = mem_map[UNDER_0xA];
                mem_map[0xB] = mem_map[UNDER_0xB];
                SetRAM(0xa000, 0xbfff);
            }
            else 
            {
                /* Enable BASIC ROM */
                mem_map[UNDER_0xA] = mem_map[0xA];
                mem_map[UNDER_0xB] = mem_map[0xB];
                mem_map[0xA] = ROM_basic + 0x0000 - 0xA000;
                mem_map[0xB] = ROM_basic + 0x1000 - 0xB000;
                SetROM(0xa000, 0xbfff);
            }
        }
    }

    /* Enable/disable Self Test ROM in 0x5000-0x57ff */
    /* Note: in Compy Shop bit 5 (ANTIC access) disables Self Test */
    if ((byte & 0x80) || ((ram_size == 576) && (byte & 0x20) == 0))
    {
        if (selftest_enabled)
        {
            /* Disable Self Test ROM */
            memcpy(memory + 0x5000, under_atarixl_os + 0x1000, 0x800);
            SetRAM(0x5000, 0x57ff);
            selftest_enabled = FALSE;
        }
    }
    else 
    {
        /* We can enable Self Test only if the OS ROM is enabled and we are not 576K or 1088K */
        if (!selftest_enabled && (byte & 0x01) && 
               !((byte & 0x30) != 0x30 && ram_size == 576) &&
               !((byte & 0x10) == 0 && ram_size == 1088)) 
        {
            /* Enable Self Test ROM */
            memcpy(under_atarixl_os + 0x1000, memory + 0x5000, 0x800);
            SetROM(0x5000, 0x57ff);
            memcpy(memory + 0x5000, atari_os + 0x1000, 0x800);
            selftest_enabled = TRUE;
        }
    }
}

// -----------------------------------------------
// Disable the Cart memory from 0x8000 to 0x9FFF 
// -----------------------------------------------
void Cart809F_Disable(void)
{
    if (cart809F_enabled) 
    {
        /* Restore 0x8000-0x9fff RAM */
        mem_map[0x8] = mem_map[UNDER_0x8];
        mem_map[0x9] = mem_map[UNDER_0x9];
        SetRAM(0x8000, 0x9fff);
        cart809F_enabled = FALSE;
    }
}

// -----------------------------------------------
// Enable the Cart memory from 0x8000 to 0x9FFF 
// -----------------------------------------------
void Cart809F_Enable(void)
{
    if (!cart809F_enabled) 
    {
        /* Back-up 0x8000-0x9fff RAM */
        mem_map[UNDER_0x8] = mem_map[0x8];
        mem_map[UNDER_0x9] = mem_map[0x9];
        SetROM(0x8000, 0x9fff);
        cart809F_enabled = TRUE;
    }
}

// -----------------------------------------------
// Disable the Cart memory from 0xA000 to 0xBFFF 
// -----------------------------------------------
void CartA0BF_Disable(void)
{
    if (cartA0BF_enabled) 
    {
        /* No BASIC if not XL/XE or bit 1 of PORTB set */
        if ((machine_type != MACHINE_XLXE) || basic_disabled((UBYTE) (PORTB | PORTB_mask))) 
        {
            mem_map[0xA] = mem_map[UNDER_0xA];
            mem_map[0xB] = mem_map[UNDER_0xB];
            SetRAM(0xa000, 0xbfff);
        }
        else
        {
            mem_map[0xA] = ROM_basic + 0x0000 - 0xA000;
            mem_map[0xB] = ROM_basic + 0x1000 - 0xB000;
            SetROM(0xa000, 0xbfff);
        }
        cartA0BF_enabled = FALSE;
        if (machine_type == MACHINE_XLXE) 
        {
            TRIG[3] = 0;
            if (GRACTL & 4)
                TRIG_latch[3] = 0;
        }
    }
}

// -----------------------------------------------
// Enable the Cart memory from 0xA000 to 0xBFFF 
// -----------------------------------------------
void CartA0BF_Enable(void)
{
    if (!cartA0BF_enabled) 
    {
        /* No BASIC if not XL/XE or bit 1 of PORTB set */
        /* or accessing extended 576K or 1088K memory */
        if (ram_size > 40 && ((machine_type != MACHINE_XLXE) || (PORTB & 0x02) || ((PORTB & 0x10) == 0 && (ram_size == 576 || ram_size == 1088)))) 
        {
            /* Back-up 0xa000-0xbfff RAM */
            mem_map[UNDER_0xA] = mem_map[0xA];
            mem_map[UNDER_0xB] = mem_map[0xB];
            SetROM(0xa000, 0xbfff);
        }
        cartA0BF_enabled = TRUE;
        if (machine_type == MACHINE_XLXE)
            TRIG[3] = 1;
    }
}


inline void CopyFromMem(UWORD from, UBYTE *to, int size)
{
    while (--size >= 0) {
        *to++ = GetByte(from);
        from++;
    }
}

inline void CopyToMem(const UBYTE *from, UWORD to, int size)
{
    while (--size >= 0) {
        PutByte(to, *from);
        from++;
        to++;
    }
}


// End of file

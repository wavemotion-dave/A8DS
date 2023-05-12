/*
 * ESC.C contains the handling for all Atari 800 Escape Codes used to patch the OS
 * This is mostly for disk speedup - which can be toggled on/off in Options (gear icon).
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
 * esc.c - Patch the OS with escape sequences
 *
 * Copyright (c) 1995-1998 David Firth
 * Copyright (c) 1998-2008 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "atari.h"
#include "cassette.h"
#include "cpu.h"
#include "devices.h"
#include "esc.h"
#include "memory.h"
#include "pia.h"
#include "sio.h"
#include <stdlib.h>


/* Now we check address of every escape code, to make sure that the patch
   has been set by the emulator and is not a CIM in Atari program.
   Also switch() for escape codes has been changed to array of pointers
   to functions. This allows adding port-specific patches (e.g. modem device)
   using ESC_Add, Devices_UpdateHATABSEntry etc. without modifying
   atari.c/devices.c. Unfortunately it can't be done for patches in Atari OS,
   because the OS in XL/XE can be disabled.
*/
static UWORD esc_address[256];
static ESC_FunctionType esc_function[256];

void ESC_ClearAll(void)
{
	int i;
	for (i = 0; i < 256; i++)
		esc_function[i] = NULL;
}

void ESC_Add(UWORD address, UBYTE esc_code, ESC_FunctionType function)
{
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xf2);			/* ESC */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
}

void ESC_AddEscRts(UWORD address, UBYTE esc_code, ESC_FunctionType function)
{
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xf2);			/* ESC */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
	dPutByte(address + 2, 0x60);		/* RTS */
}

/* 0xd2 is ESCRTS, which works same as pair of ESC and RTS (I think so...).
   So this function does effectively the same as ESC_AddEscRts,
   except that it modifies 2, not 3 bytes in Atari memory.
   I don't know why it is done that way, so I simply leave it
   unchanged (0xf2/0xd2 are used as in previous versions).
*/
void ESC_AddEscRts2(UWORD address, UBYTE esc_code, ESC_FunctionType function)
{
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xd2);			/* ESCRTS */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
}

void ESC_Remove(UBYTE esc_code)
{
	esc_function[esc_code] = NULL;
}

void ESC_Run(UBYTE esc_code)
{
	if (esc_address[esc_code] == CPU_regPC - 2 && esc_function[esc_code] != NULL) {
		esc_function[esc_code]();
		return;
	}
	CPU_cim_encountered = 1;
	Atari800_Exit(TRUE);
}

void ESC_PatchOS(void)
{
	int patched = Devices_PatchOS();
	if (myConfig.disk_speedup) {
		UWORD addr_l;
		UWORD addr_s;
		UBYTE check_s_0;
		UBYTE check_s_1;
		/* patch Open() of C: so we know when a leader is processed */
		switch (Atari800_machine_type) 
        {
		case Atari800_MACHINE_OSA:
		case Atari800_MACHINE_OSB:
            if (myConfig.os_type == OS_ALTIRRA_800)
			    addr_l = 0xef91; /* Altirra Cassettle Handler*/
            else
                addr_l = 0xef74; /* Atari */                
			addr_s = 0xefbc;
			check_s_0 = 0xa0;
			check_s_1 = 0x80;
			break;
		case Atari800_MACHINE_XLXE:
            if (myConfig.os_type == OS_ALTIRRA_XL)
            {
                addr_l = 0xee4a;
            }
            else
            {
	    		addr_l = 0xfd13;
            }
			addr_s = 0xfd60;
			check_s_0 = 0xa9;
			check_s_1 = 0x03;
			break;
		default:
			return;
		}
		/* don't hurt non-standard OSes that may not support cassette at all  */
		if (dGetByte(addr_l)     == 0xa9 && dGetByte(addr_l + 1) == 0x03
		 && dGetByte(addr_l + 2) == 0x8d && dGetByte(addr_l + 3) == 0x2a
		 && dGetByte(addr_l + 4) == 0x02
		 && dGetByte(addr_s)     == check_s_0
		 && dGetByte(addr_s + 1) == check_s_1
		 && dGetByte(addr_s + 2) == 0x20 && dGetByte(addr_s + 3) == 0x5c
		 && dGetByte(addr_s + 4) == 0xe4) {
			ESC_Add(addr_l, ESC_COPENLOAD, CASSETTE_LeaderLoad);
			ESC_Add(addr_s, ESC_COPENSAVE, CASSETTE_LeaderSave);
		}
		ESC_AddEscRts(0xe459, ESC_SIOV, SIO_Handler);
		patched = TRUE;
	}
	else {
		ESC_Remove(ESC_COPENLOAD);
		ESC_Remove(ESC_COPENSAVE);
		ESC_Remove(ESC_SIOV);
	};
	if (patched && Atari800_machine_type == Atari800_MACHINE_XLXE) 
    {
		/* Disable Checksum Test */
		if (myConfig.os_type != OS_ALTIRRA_XL)
		{
		  dPutByte(0xc31d, 0xea);
		  dPutByte(0xc31e, 0xea);
		}
		//dPutByte(0xc319, 0x8e);
		//dPutByte(0xc31a, 0xff);
	}
}

void ESC_UpdatePatches(void)
{
	switch (Atari800_machine_type) {
	case Atari800_MACHINE_OSA:
	case Atari800_MACHINE_OSB:
		/* Restore unpatched OS */
		dCopyToMem(atari_os, 0xd800, 0x2800);
		/* Set patches */
		ESC_PatchOS();
		Devices_UpdatePatches();
		break;
	case Atari800_MACHINE_XLXE:
		/* Don't patch if OS disabled */
		if ((PIA_PORTB & 1) == 0)
			break;
		/* Restore unpatched OS */
		dCopyToMem(atari_os, 0xc000, 0x1000);
		dCopyToMem(atari_os + 0x1800, 0xd800, 0x2800);
		/* Set patches */
		ESC_PatchOS();
		Devices_UpdatePatches();
		break;
	default:
		break;
	}
}

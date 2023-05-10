/*
 * ATARI.C contains the basic machine settings for the emulator. 
 * This defines how much RAM and whether the machine is compatible
 * with the older stock Atari 800 or the newer XL/XE series (possibly
 * with extended RAM banks).  This routine also sorts out the various
 * supported ROM file types - right now, ATR, ATX and XEX are the only
 * files supported (which will run 99% of everything 8-bit).
 *
 * XEGS-DS - Atari 8-bit Emulator designed to run 8-bit games on the Nintendo DS/DSi
 * Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */

/*
 * atari.c - main high-level routines
 *
 * Copyright (c) 1995-1998 David Firth
 * Copyright (c) 1998-2006 Atari800 development team (see DOC/CREDITS)
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
#include "antic.h"
#include "atari.h"
#include "esc.h"
#include "binload.h"
#include "cartridge.h"
#include "cassette.h"
#include "cpu.h"
#include "devices.h"
#include "gtia.h"
#include "input.h"
#include "memory.h"
#include "pia.h"
#include "platform.h"
#include "pokeysnd.h"
#include "rtime.h"
#include "sio.h"
#include "util.h"
#include "pokeysnd.h"

// ----------------------------------------------------------------------------------
// Some basic machine settings... we default to an XEGS with 128K of RAM installed.
// This is a bit of a "custom" machine but is capable of running 98% of all games.
// ----------------------------------------------------------------------------------
int machine_type     = MACHINE_XLXE;
int ram_size         = RAM_128K;        // We only allow RAM_128K or RAM_320_RAMBO and for backwards compatibility RAM_48K
int tv_mode          = TV_NTSC;
int disable_basic    = TRUE;
int skip_frames      = FALSE;
int file_type        = AFILE_ERROR;

char disk_filename[DISK_MAX][256];
int  disk_readonly[DISK_MAX] = {true,true,true};

void Warmstart(void) 
{
	if (machine_type == MACHINE_OSA || machine_type == MACHINE_OSB) 
    {
		/* RESET key in 400/800 does not reset chips,
		   but only generates RNMI interrupt */
		NMIST = 0x3f;
		NMI();
	}
	else 
    {
		PIA_Reset();
		ANTIC_Reset();
		/* CPU_Reset() must be after PIA_Reset(),
		   because Reset routine vector must be read from OS ROM */
		CPU_Reset();
		/* note: POKEY and GTIA have no Reset pin */
	}
}

void Coldstart(void) 
{
	PIA_Reset();
	ANTIC_Reset();
	/* CPU_Reset() must be after PIA_Reset(),
	   because Reset routine vector must be read from OS ROM */
	CPU_Reset();
	/* note: POKEY and GTIA have no Reset pin */
	/* reset cartridge to power-up state */
	CART_Start();

	/* set Atari OS Coldstart flag */
	dPutByte(0x244, 1);
	/* handle Option key (disable BASIC in XL/XE)
	   and Start key (boot from cassette) */
	consol_index = 2;
	consol_table[2] = 0x0f;
	if (disable_basic && !BINLOAD_loading_basic) {
		/* hold Option during reboot */
		consol_table[2] &= ~CONSOL_OPTION;
	}
	consol_table[1] = consol_table[2];
}

int Atari800_InitialiseMachine(void) 
{
	ESC_ClearAll();
	MEMORY_InitialiseMachine();
	Devices_UpdatePatches();
	return TRUE;
}

int Atari800_DetectFileType(const char *filename) 
{
    // Nothing fancy here... if the filename says it's ATR or XEX who are we to argue...
    if (strstr(filename, ".atr") != 0) return  AFILE_ATR;
    if (strstr(filename, ".Atr") != 0) return  AFILE_ATR;
    if (strstr(filename, ".ATR") != 0) return  AFILE_ATR;
    if (strstr(filename, ".atx") != 0) return  AFILE_ATX;
    if (strstr(filename, ".ATX") != 0) return  AFILE_ATX;
    if (strstr(filename, ".xex") != 0) return  AFILE_XEX;
    if (strstr(filename, ".XEX") != 0) return  AFILE_XEX;
    if (strstr(filename, ".car") != 0) return  AFILE_CART;
    if (strstr(filename, ".CAR") != 0) return  AFILE_CART;
    if (strstr(filename, ".rom") != 0) return  AFILE_ROM;
    if (strstr(filename, ".ROM") != 0) return  AFILE_ROM;
	return AFILE_ERROR;
}

int Atari800_OpenFile(const char *filename, int reboot, int diskno, int readonly, int bEnableBasic) 
{
    memset((UBYTE*)bgGetGfxPtr(bg2), 0x00, 128*1024);
    
	file_type = Atari800_DetectFileType(filename);
    
	switch (file_type) 
    {
    case AFILE_ATR:
    case AFILE_ATX:
      if (reboot)   // If we are booting a disk, empty out the XEX filename
      {
          CART_Insert(bEnableBasic, file_type, filename); 
          strcpy(disk_filename[DISK_XEX], "EMPTY");
      } // Else do not force any CART change here... needed so we can load a disk  on top of a CART
      strcpy(disk_filename[diskno], filename);
      disk_readonly[diskno] = readonly;
      if (!SIO_Mount(diskno, filename, readonly))
        return AFILE_ERROR;
      if (reboot)
        Coldstart();
      break;
    case AFILE_XEX:
      CART_Insert(bEnableBasic, file_type, filename); 
      strcpy(disk_filename[DISK_1], "EMPTY");
      strcpy(disk_filename[DISK_2], "EMPTY");
      strcpy(disk_filename[DISK_XEX], filename);
      if (!BINLOAD_Loader(filename))
        return AFILE_ERROR;
      break;
    case AFILE_CART:
      CART_Insert(bEnableBasic, file_type, filename); 
      strcpy(disk_filename[DISK_1], "EMPTY");
      strcpy(disk_filename[DISK_2], "EMPTY");
      strcpy(disk_filename[DISK_XEX], filename);
      Atari800_Coldstart();
      break;
    case AFILE_ROM:
      CART_Insert(bEnableBasic, file_type, filename); 
      strcpy(disk_filename[DISK_1], "EMPTY");
      strcpy(disk_filename[DISK_2], "EMPTY");
      strcpy(disk_filename[DISK_XEX], filename);
      Atari800_Coldstart();
      break;
	}
	return file_type;
}

int Atari800_Initialise(void) 
{
    int argc=0;
    char *argv[]={""};
    Devices_Initialise(&argc, argv);
    RTIME_Initialise();
    SIO_Initialise (&argc, argv);
    
    strcpy(disk_filename[DISK_XEX], "EMPTY");
    strcpy(disk_filename[DISK_1], "EMPTY");
    strcpy(disk_filename[DISK_2], "EMPTY");    
    
    disk_readonly[DISK_XEX] = true;
    disk_readonly[DISK_1] = true;
    disk_readonly[DISK_2] = true;
    
    if (ram_size == RAM_48K)
        machine_type     = MACHINE_OSB;
    else
        machine_type     = MACHINE_XLXE;

    INPUT_Initialise();

    // Platform Specific Initialisation 
    Atari_Initialise();

    // Initialise Custom Chips
    ANTIC_Initialise();
    GTIA_Initialise();
    PIA_Initialise();
    POKEY_Initialise();

    Atari800_InitialiseMachine();

    return TRUE;
}

int Atari800_Exit(int run_monitor) 
{
    extern bool bAtariCrash;
	int restart;

	restart = Atari_Exit(run_monitor);
	if (!restart) 
    {
		SIO_Exit();	/* umount disks, so temporary files are deleted */
	}
    
    //TBD-DSB For now we are keeping the system alive. 
    // Mainly so the user can click into another game 
    // rather than have the emulator just exit.
    bAtariCrash = true;
    
    restart = 1;
	return restart;
}

UBYTE Atari800_GetByte(UWORD addr) 
{
    switch (addr & 0xff00) 
    {
    case 0xd000:				/* GTIA */
        return GTIA_GetByte(addr);
        break;
    case 0xd200:				/* POKEY */
        return POKEY_GetByte(addr);
        break;
    case 0xd300:				/* PIA */
        return PIA_GetByte(addr);
        break;
    case 0xd400:				/* ANTIC */
        return ANTIC_GetByte(addr);
        break;
    }
    return 0xFF;
}

void Atari800_PutByte(UWORD addr, UBYTE byte) 
{
	switch (addr & 0xff00) 
    {
    case 0xd000:				/* GTIA */
      GTIA_PutByte(addr, byte);
      break;
    case 0xd200:				/* POKEY */
      POKEY_PutByte(addr, byte);
      break;
    case 0xd300:				/* PIA */
      PIA_PutByte(addr, byte);
      break;
    case 0xd400:				/* ANTIC */
      ANTIC_PutByte(addr, byte);
      break;
	}
}

void Atari800_UpdatePatches(void) {
	switch (machine_type) {
	case MACHINE_OSA:
	case MACHINE_OSB:
		/* Restore unpatched OS */
		dCopyToMem(atari_os, 0xd800, 0x2800);
		/* Set patches */
		ESC_PatchOS();
		Devices_UpdatePatches();
		break;
	case MACHINE_XLXE:
		/* Don't patch if OS disabled */
		if ((PORTB & 1) == 0)
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

void Atari800_Frame() 
{
	Devices_Frame();
	INPUT_Frame();
	GTIA_Frame();
    ANTIC_Frame(skip_frames ? (gTotalAtariFrames & (skip_frames==1 ? 0x03:0x01)) : TRUE);  // Skip every 4th frame... or every other frame if we are "aggressive"
    POKEY_Frame();
    
    gTotalAtariFrames++;
}

void MainStateSave(void) {}
void MainStateRead(void) {}

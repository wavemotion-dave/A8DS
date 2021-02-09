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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# elif defined(HAVE_TIME_H)
#  include <time.h>
# endif
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WIN32
#include <windows.h>
#endif
#ifdef __EMX__
#define INCL_DOS
#include <os2.h>
#endif
#ifdef __BEOS__
#include <OS.h>
#endif
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

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
#if !defined(BASIC) && !defined(CURSES_BASIC)
//#include "colours.h"
#include "screen.h"
#endif
#ifndef BASIC
#include "statesav.h"
#ifndef __PLUS
//#include "ui.h"
#endif
#endif /* BASIC */
#if defined(SOUND) && !defined(__PLUS)
#include "pokeysnd.h"
#include "sound.h"
#endif

#ifdef __PLUS
#ifdef _WX_
#include "export.h"
#else /* _WX_ */
#include "globals.h"
#include "macros.h"
#include "display_win.h"
#include "misc_win.h"
#include "registry.h"
#include "timing.h"
#include "FileService.h"
#include "Helpers.h"
#endif /* _WX_ */
#endif /* __PLUS */

#include "global.h"

int machine_type = MACHINE_XLXE;
int ram_size = 128;
int tv_mode = TV_NTSC;
int disable_basic = TRUE;
int enable_sio_patch = TRUE;

int verbose = FALSE;

int sprite_collisions_in_skipped_frames = FALSE;

int skip_frames = FALSE;

void Warmstart(void) {
	if (machine_type == MACHINE_OSA || machine_type == MACHINE_OSB) {
		/* RESET key in 400/800 does not reset chips,
		   but only generates RNMI interrupt */
		NMIST = 0x3f;
		NMI();
	}
	else {
		PIA_Reset();
		ANTIC_Reset();
		/* CPU_Reset() must be after PIA_Reset(),
		   because Reset routine vector must be read from OS ROM */
		CPU_Reset();
		/* note: POKEY and GTIA have no Reset pin */
	}
#ifdef __PLUS
	HandleResetEvent();
#endif
}

void Coldstart(void) {
	PIA_Reset();
	ANTIC_Reset();
	/* CPU_Reset() must be after PIA_Reset(),
	   because Reset routine vector must be read from OS ROM */
	CPU_Reset();
	/* note: POKEY and GTIA have no Reset pin */
#ifdef __PLUS
	HandleResetEvent();
#endif
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
	if (CASSETTE_hold_start) {
		/* hold Start during reboot */
		consol_table[2] &= ~CONSOL_START;
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
    if (strstr(filename, ".ATR") != 0) return  AFILE_ATR;
    if (strstr(filename, ".xex") != 0) return  AFILE_XEX;
    if (strstr(filename, ".XEX") != 0) return  AFILE_XEX;
	return AFILE_ERROR;
}

int Atari800_OpenFile(const char *filename, int reboot, int diskno, int readonly, int bEnableBasic) 
{
    CART_Insert(bEnableBasic);
  
	int type = Atari800_DetectFileType(filename);

	switch (type) 
    {
    case AFILE_ATR:
      if (!SIO_Mount(diskno, filename, readonly))
        return AFILE_ERROR;
      if (reboot)
        Coldstart();
      break;
    case AFILE_XEX:
      if (!BINLOAD_Loader(filename))
        return AFILE_ERROR;
      break;
	}
	return type;
}

int Atari800_Initialise(void) 
{
    int argc=0;
    char *argv[]={""};
    Devices_Initialise(&argc, argv);
    RTIME_Initialise();
    SIO_Initialise (&argc, argv);
    CASSETTE_Initialise(&argc, argv);

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

UNALIGNED_STAT_DEF(atari_screen_write_long_stat)
UNALIGNED_STAT_DEF(pm_scanline_read_long_stat)
UNALIGNED_STAT_DEF(memory_read_word_stat)
UNALIGNED_STAT_DEF(memory_write_word_stat)
UNALIGNED_STAT_DEF(memory_read_aligned_word_stat)
UNALIGNED_STAT_DEF(memory_write_aligned_word_stat)

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
	UBYTE byte = 0xff;
	switch (addr & 0xff00) {
	case 0xd000:				/* GTIA */
	case 0xc000:				/* GTIA - 5200 */
		byte = GTIA_GetByte(addr);
		break;
	case 0xd200:				/* POKEY */
	case 0xe800:				/* POKEY - 5200 */
	case 0xeb00:				/* POKEY - 5200 */
	  byte = POKEY_GetByte(addr);
		break;
	case 0xd300:				/* PIA */
		byte = PIA_GetByte(addr);
		break;
	case 0xd400:				/* ANTIC */
		byte = ANTIC_GetByte(addr);
		break;
	default:
		break;
	}

	return byte;
}

void Atari800_PutByte(UWORD addr, UBYTE byte) {
	switch (addr & 0xff00) 
    {
    case 0xd000:				/* GTIA */
    case 0xc000:				/* GTIA - 5200 */
      GTIA_PutByte(addr, byte);
      break;
    case 0xd200:				/* POKEY */
    case 0xe800:				/* POKEY - 5200 AAA added other pokey space */
    case 0xeb00:				/* POKEY - 5200 */
      POKEY_PutByte(addr, byte);
      break;
    case 0xd300:				/* PIA */
      PIA_PutByte(addr, byte);
      break;
    case 0xd400:				/* ANTIC */
      ANTIC_PutByte(addr, byte);
      break;
    default:
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
    ANTIC_Frame(skip_frames ? (gTotalAtariFrames & 3) : TRUE);  // Skip every 4th frame...
    POKEY_Frame();
    
    gTotalAtariFrames++;
}

#ifndef BASIC

void MainStateSave(void) {
	UBYTE temp;
	int default_tv_mode;
	int os = 0;
	int default_system = 3;
	int pil_on = FALSE;

	if (tv_mode == TV_PAL) {
		temp = 0;
		default_tv_mode = 1;
	}
	else {
		temp = 1;
		default_tv_mode = 2;
	}
	SaveUBYTE(&temp, 1);

	switch (machine_type) {
	case MACHINE_OSA:
		temp = ram_size == 16 ? 5 : 0;
		os = 1;
		default_system = 1;
		break;
	case MACHINE_OSB:
		temp = ram_size == 16 ? 5 : 0;
		os = 2;
		default_system = 2;
		break;
	case MACHINE_XLXE:
		switch (ram_size) {
		case 16:
			temp = 6;
			default_system = 3;
			break;
		case 64:
			temp = 1;
			default_system = 3;
			break;
		case 128:
			temp = 2;
			default_system = 4;
			break;
		case 192:
			temp = 9;
			default_system = 8;
			break;
		case RAM_320_RAMBO:
		case RAM_320_COMPY_SHOP:
			temp = 3;
			default_system = 5;
			break;
		case 576:
			temp = 7;
			default_system = 6;
			break;
		case 1088:
			temp = 8;
			default_system = 7;
			break;
		}
		break;
	case MACHINE_5200:
		temp = 4;
		default_system = 6;
		break;
	}
	SaveUBYTE(&temp, 1);

	SaveINT(&os, 1);
	SaveINT(&pil_on, 1);
	SaveINT(&default_tv_mode, 1);
	SaveINT(&default_system, 1);
}

void MainStateRead(void) {
	/* these are all for compatibility with previous versions */
	UBYTE temp;
	int default_tv_mode;
	int os;
	int default_system;
	int pil_on;

	ReadUBYTE(&temp, 1);
	tv_mode = (temp == 0) ? TV_PAL : TV_NTSC;

	ReadUBYTE(&temp, 1);
	ReadINT(&os, 1);
	switch (temp) {
	case 0:
		machine_type = os == 1 ? MACHINE_OSA : MACHINE_OSB;
		ram_size = 48;
		break;
	case 1:
		machine_type = MACHINE_XLXE;
		ram_size = 64;
		break;
	case 2:
		machine_type = MACHINE_XLXE;
		ram_size = 128;
		break;
	case 3:
		machine_type = MACHINE_XLXE;
		ram_size = RAM_320_COMPY_SHOP;
		break;
	case 4:
		machine_type = MACHINE_5200;
		ram_size = 16;
		break;
	case 5:
		machine_type = os == 1 ? MACHINE_OSA : MACHINE_OSB;
		ram_size = 16;
		break;
	case 6:
		machine_type = MACHINE_XLXE;
		ram_size = 16;
		break;
	case 7:
		machine_type = MACHINE_XLXE;
		ram_size = 576;
		break;
	case 8:
		machine_type = MACHINE_XLXE;
		ram_size = 1088;
		break;
	case 9:
		machine_type = MACHINE_XLXE;
		ram_size = 192;
		break;
	default:
		machine_type = MACHINE_XLXE;
		ram_size = 64;
		iprintf("Warning: Bad machine type read in from state save, defaulting to 800 XL");
		break;
	}

	ReadINT(&pil_on, 1);
	ReadINT(&default_tv_mode, 1);
	ReadINT(&default_system, 1);

//	load_roms();
	/* XXX: what about patches? */
}

#endif


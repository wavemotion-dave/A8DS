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


//int emuos_mode = 1;	/* 0 = never use EmuOS, 1 = use EmuOS if real OS not available, 2 = always use EmuOS */

/* Now we check address of every escape code, to make sure that the patch
   has been set by the emulator and is not a CIM in Atari program.
   Also switch() for escape codes has been changed to array of pointers
   to functions. This allows adding port-specific patches (e.g. modem device)
   using Atari800_AddEsc, Device_UpdateHATABSEntry etc. without modifying
   atari.c/devices.c. Unfortunately it can't be done for patches in Atari OS,
   because the OS in XL/XE can be disabled.
*/
static UWORD esc_address[256];
static EscFunctionType esc_function[256];

void Atari800_ClearAllEsc(void) {
	int i;
	for (i = 0; i < 256; i++)
		esc_function[i] = NULL;
}

void Atari800_AddEsc(UWORD address, UBYTE esc_code, EscFunctionType function) {
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xf2);			/* ESC */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
}

void Atari800_AddEscRts(UWORD address, UBYTE esc_code, EscFunctionType function) {
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xf2);			/* ESC */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
	dPutByte(address + 2, 0x60);		/* RTS */
}

/* 0xd2 is ESCRTS, which works same as pair of ESC and RTS (I think so...).
   So this function does effectively the same as Atari800_AddEscRts,
   except that it modifies 2, not 3 bytes in Atari memory.
   I don't know why it is done that way, so I simply leave it
   unchanged (0xf2/0xd2 are used as in previous versions).
*/
void Atari800_AddEscRts2(UWORD address, UBYTE esc_code, EscFunctionType function)
{
	esc_address[esc_code] = address;
	esc_function[esc_code] = function;
	dPutByte(address, 0xd2);			/* ESCRTS */
	dPutByte(address + 1, esc_code);	/* ESC CODE */
}

void Atari800_RemoveEsc(UBYTE esc_code)
{
	esc_function[esc_code] = NULL;
}

void Atari800_RunEsc(UBYTE esc_code)
{
	if (esc_address[esc_code] == regPC - 2 && esc_function[esc_code] != NULL) {
		esc_function[esc_code]();
		return;
	}
#ifdef CRASH_MENU
	regPC -= 2;
	crash_address = regPC;
	crash_afterCIM = regPC + 2;
	crash_code = dGetByte(crash_address);
	ui();
#else /* CRASH_MENU */
	cim_encountered = 1;
	iprintf("Invalid ESC code %02x at address %04x", esc_code, regPC - 2);
#ifndef __PLUS
	if (!Atari800_Exit(TRUE))
		exit(0);
#else /* __PLUS */
	Atari800_Exit(TRUE);
#endif /* __PLUS */
#endif /* CRASH_MENU */
}

void Atari800_PatchOS(void) {
	int patched = Device_PatchOS();

	if (enable_sio_patch) {
		UWORD addr_l;
		UWORD addr_s;
		UBYTE check_s_0;
		UBYTE check_s_1;
		/* patch Open() of C: so we know when a leader is processed */
		switch (machine_type) {
		case MACHINE_OSA:
		case MACHINE_OSB:
			addr_l = 0xef74;
			addr_s = 0xefbc;
			check_s_0 = 0xa0;
			check_s_1 = 0x80;
			break;
		case MACHINE_XLXE:
			addr_l = 0xfd13;
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
			Atari800_AddEsc(addr_l, ESC_COPENLOAD, CASSETTE_LeaderLoad);
			Atari800_AddEsc(addr_s, ESC_COPENSAVE, CASSETTE_LeaderSave);
		}
		Atari800_AddEscRts(0xe459, ESC_SIOV, SIO);
		patched = TRUE;
	}
	else {
		Atari800_RemoveEsc(ESC_COPENLOAD);
		Atari800_RemoveEsc(ESC_COPENSAVE);
		Atari800_RemoveEsc(ESC_SIOV);
	};
	if (patched && machine_type == MACHINE_XLXE) {
		/* Disable Checksum Test */
		dPutByte(0xc314, 0x8e);
		dPutByte(0xc315, 0xff);
		dPutByte(0xc319, 0x8e);
		dPutByte(0xc31a, 0xff);
	}
}

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
	if (disable_basic && !loading_basic) {
		/* hold Option during reboot */
		consol_table[2] &= ~CONSOL_OPTION;
	}
	if (hold_start) {
		/* hold Start during reboot */
		consol_table[2] &= ~CONSOL_START;
	}
	consol_table[1] = consol_table[2];
}

int Atari800_InitialiseMachine(void) 
{
	Atari800_ClearAllEsc();
	MEMORY_InitialiseMachine();
	Device_UpdatePatches();
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
      if (!BIN_loader(filename))
        return AFILE_ERROR;
      break;
	}
	return type;
}

int Atari800_Initialise(void) 
{
    Device_Initialise();
    RTIME_Initialise();
    SIO_Initialise ();
    CASSETTE_Initialise();

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
		Atari800_PatchOS();
		Device_UpdatePatches();
		break;
	case MACHINE_XLXE:
		/* Don't patch if OS disabled */
		if ((PORTB & 1) == 0)
			break;
		/* Restore unpatched OS */
		dCopyToMem(atari_os, 0xc000, 0x1000);
		dCopyToMem(atari_os + 0x1800, 0xd800, 0x2800);
		/* Set patches */
		Atari800_PatchOS();
		Device_UpdatePatches();
		break;
	default:
		break;
	}
}

void Atari800_Frame() 
{
	Device_Frame();
	INPUT_Frame();
	GTIA_Frame();
    ANTIC_Frame(TRUE);
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


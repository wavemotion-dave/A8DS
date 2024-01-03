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
 * Copyright (c) 2021-2024 Dave Bernazzani (wavemotion-dave)

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

#ifndef ESC_H_
#define ESC_H_

/* Escape codes used to mark places in 6502 code that must
   be handled specially by the emulator. An escape sequence
   is an illegal 6502 opcode 0xF2 or 0xD2 followed
   by one of these escape codes: */
enum ESC_t {

    /* SIO patch. */
    ESC_SIOV,

    /* stdio-based handlers for the BASIC version
       and handlers for Atari Basic loader. */
    ESC_EHOPEN,
    ESC_EHCLOS,
    ESC_EHREAD,
    ESC_EHWRIT,
    ESC_EHSTAT,
    ESC_EHSPEC,

    ESC_KHOPEN,
    ESC_KHCLOS,
    ESC_KHREAD,
    ESC_KHWRIT,
    ESC_KHSTAT,
    ESC_KHSPEC,

    /* Atari executable loader. */
    ESC_BINLOADER_CONT,

    /* Cassette emulation. */
    ESC_COPENLOAD = 0xa8,
    ESC_COPENSAVE = 0xa9,

    /* Printer. */
    ESC_PHOPEN = 0xb0,
    ESC_PHCLOS = 0xb1,
    ESC_PHREAD = 0xb2,
    ESC_PHWRIT = 0xb3,
    ESC_PHSTAT = 0xb4,
    ESC_PHSPEC = 0xb5,
    ESC_PHINIT = 0xb6,

#ifdef R_IO_DEVICE
    /* R: device. */
    ESC_ROPEN = 0xd0,
    ESC_RCLOS = 0xd1,
    ESC_RREAD = 0xd2,
    ESC_RWRIT = 0xd3,
    ESC_RSTAT = 0xd4,
    ESC_RSPEC = 0xd5,
    ESC_RINIT = 0xd6,
#endif

    /* H: device. */
    ESC_HHOPEN = 0xc0,
    ESC_HHCLOS = 0xc1,
    ESC_HHREAD = 0xc2,
    ESC_HHWRIT = 0xc3,
    ESC_HHSTAT = 0xc4,
    ESC_HHSPEC = 0xc5,
    ESC_HHINIT = 0xc6,

    /* B: device. */
    ESC_BOPEN = 0xe0,
    ESC_BCLOS = 0xe1,
    ESC_BREAD = 0xe2,
    ESC_BWRIT = 0xe3,
    ESC_BSTAT = 0xe4,
    ESC_BSPEC = 0xe5,
    ESC_BINIT = 0xe6
};

/* A function called to handle an escape sequence. */
typedef void (*ESC_FunctionType)(void);

/* Puts an escape sequence at the specified address. */
void ESC_Add(UWORD address, UBYTE esc_code, ESC_FunctionType function);

/* Puts an escape sequence followed by the RTS instruction. */
void ESC_AddEscRts(UWORD address, UBYTE esc_code, ESC_FunctionType function);

/* Puts an escape sequence with an integrated RTS. */
void ESC_AddEscRts2(UWORD address, UBYTE esc_code, ESC_FunctionType function);

/* Unregisters an escape sequence. You must cleanup the Atari memory yourself. */
void ESC_Remove(UBYTE esc_code);

/* Handles an escape sequence. */
void ESC_Run(UBYTE esc_code);

/* Installs SIO patch and disables ROM checksum test. */
void ESC_PatchOS(void);

/* Unregisters all escape sequences */
void ESC_ClearAll(void);

/* Reinitializes patches after enable_*_patch change. */
void ESC_UpdatePatches(void);


extern int CASSETTE_hold_start_on_reboot;
extern int CASSETTE_hold_start;
extern int CASSETTE_press_space;

extern void CASSETTE_LeaderLoad(void);
extern void CASSETTE_LeaderSave(void);
extern int CASSETTE_GetByte(void);
extern int CASSETTE_IOLineStatus(void);
extern int CASSETTE_GetInputIRQDelay(void);
extern int CASSETTE_IsSaveFile(void);
extern void CASSETTE_PutByte(int byte);
extern void CASSETTE_TapeMotor(int onoff);


#endif /* ESC_H_ */

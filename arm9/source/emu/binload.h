/*
 * BINLOAD.C contains code to load a binary executable into the Atari memory.
 * 
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly recommended you seek out the latest Atari800 sources.
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
 * binload.c - load a binary executable file
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef BINLOAD_H_
#define BINLOAD_H_

#include <stdio.h> /* FILE */
#include "atari.h" /* UBYTE */

extern FILE *BINLOAD_bin_file;

int BINLOAD_Loader(const char *filename);
extern int BINLOAD_start_binloading;
extern int BINLOAD_loading_basic;

/* Set to TRUE to enable loading of XEX with approximate disk speed */
extern int BINLOAD_slow_xex_loading;

/* Indicates that a DOS file is being currently slowly loaded. */
extern int BINLOAD_wait_active;

/* Set it to TRUE to pause the current loading of a DOS file. */
extern int BINLOAD_pause_loading;

#define BINLOAD_LOADING_BASIC_SAVED              1
#define BINLOAD_LOADING_BASIC_LISTED             2
#define BINLOAD_LOADING_BASIC_LISTED_ATARI       3
#define BINLOAD_LOADING_BASIC_LISTED_CR          4
#define BINLOAD_LOADING_BASIC_LISTED_LF          5
#define BINLOAD_LOADING_BASIC_LISTED_CRLF        6
#define BINLOAD_LOADING_BASIC_LISTED_CR_OR_CRLF  7
#define BINLOAD_LOADING_BASIC_RUN                8
int BINLOAD_LoaderStart(UBYTE *buffer);

#endif /* BINLOAD_H_ */

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

#ifndef _CARTRIDGE_H_
#define _CARTRIDGE_H_

#include "atari.h"
#include "../config.h"

#define CART_NONE           0
#define CART_STD_8          1
#define CART_STD_16         2
#define CART_OSS_16_034M    3
#define CART_5200_32        4
#define CART_DB_32          5
#define CART_5200_EE_16     6
#define CART_5200_40        7
#define CART_WILL_64        8
#define CART_EXP_64         9
#define CART_DIAMOND_64     10
#define CART_SDX_64         11
#define CART_XEGS_32        12
#define CART_XEGS_64        13
#define CART_XEGS_128       14
#define CART_OSS_16         15
#define CART_5200_NS_16     16
#define CART_ATRAX_128      17
#define CART_BBSB_40        18
#define CART_5200_8         19
#define CART_5200_4         20
#define CART_RIGHT_8        21
#define CART_WILL_32        22
#define CART_XEGS_256       23
#define CART_XEGS_512       24
#define CART_XEGS_1024      25
#define CART_MEGA_16        26
#define CART_MEGA_32        27
#define CART_MEGA_64        28
#define CART_MEGA_128       29
#define CART_MEGA_256       30
#define CART_MEGA_512       31
#define CART_MEGA_1024      32
#define CART_SWXEGS_32      33
#define CART_SWXEGS_64      34
#define CART_SWXEGS_128     35
#define CART_SWXEGS_256     36
#define CART_SWXEGS_512     37
#define CART_SWXEGS_1024    38
#define CART_PHOENIX_8      39
#define CART_BLIZZARD_16    40
#define CART_ATMAX_128      41
#define CART_ATMAX_1024     42
#define CART_SDX_128        43
#define CART_OSS_8          44
#define CART_OSS_16_043M    45
#define CART_TURBOSOFT_64   50
#define CART_TURBOSOFT_128  51
#define CART_SIC_128        54
#define CART_SIC_256        55
#define CART_SIC_512        56
#define CART_STD_4          58
#define CART_ATMAX_NEW_1024 75

#define CTRL_JOY        1
#define CTRL_SWAP       2

#define DIGITAL         0
#define ANALOG          1

#define CART_MAX_SIZE   (1024 * 1024)

extern UBYTE *cart_mem_ptr;
extern UBYTE cart_image[];

int CART_Insert(int enabled, int type, const char *filename);
void CART_Remove(void);
void CART_Start(void);
UBYTE CART_GetByte(UWORD addr);
void CART_PutByte(UWORD addr, UBYTE byte);

#endif /* _CARTRIDGE_H_ */

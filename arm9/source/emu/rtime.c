/*
 * RTIME.C contains the emulation of the R-TIME 8 cartridge (for time/date support)
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
 * rtime.c - Emulate ICD R-Time 8 cartridge
 *
 * Copyright (C) 2000 Jason Duerstock
 * Copyright (C) 2000-2008 Atari800 development team (see DOC/CREDITS)
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

#define HAVE_TIME_H
#define HAVE_TIME 
#define HAVE_GETTIME

#include <stdlib.h> /* for NULL */
#include <string.h> /* for strcmp() */
#include <time.h>
#include "atari.h"

static int rtime_state = 0;
                /* 0 = waiting for register # */
                /* 1 = got register #, waiting for hi nibble */
                /* 2 = got hi nibble, waiting for lo nibble */
static int rtime_tmp = 0;
static int rtime_tmp2 = 0;

static UBYTE regset[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void RTIME_Initialise(void)
{
    // RTIME is always initialized... nothing to do....
}


static int hex2bcd(int h)
{
    return ((h / 10) << 4) | (h % 10);
}

// -------------------------------------------------------------------
// Need to get time on the Nintendo DS so we can plug it back in to 
// the Atari R-Time 8 access... this is only useful for things like
// Sparta DOS but it's simple enough and small enough code that we
// just support it.  If push came to shove, we could ditch this all.
// -------------------------------------------------------------------
static int gettime(int p)
{
    time_t tt;
    struct tm *lt;

    tt = time(NULL);
    lt = localtime(&tt);

    switch (p) 
    {
    case 0:
        return hex2bcd(lt->tm_sec);
    case 1:
        return hex2bcd(lt->tm_min);
    case 2:
        return hex2bcd(lt->tm_hour);
    case 3:
        return hex2bcd(lt->tm_mday);
    case 4:
        return hex2bcd(lt->tm_mon + 1);
    case 5:
        return hex2bcd(lt->tm_year % 100);
    case 6:
        return hex2bcd(((lt->tm_wday + 2) % 7) + 1);
    }
    return 0;
}

UBYTE RTIME_GetByte(void)
{
    switch (rtime_state) 
    {
    case 0:
        return 0;   // Pretending rtime not busy, returning 0...
    case 1:
        rtime_state = 2;
        return (rtime_tmp <= 6 ? gettime(rtime_tmp) : regset[rtime_tmp]) >> 4;
    case 2:
        rtime_state = 0;
        return (rtime_tmp <= 6 ? gettime(rtime_tmp) : regset[rtime_tmp]) & 0x0f;
    }
    return 0;
}

void RTIME_PutByte(UBYTE byte)
{
    switch (rtime_state) 
    {
    case 0:
        rtime_tmp = byte & 0x0f;
        rtime_state = 1;
        break;
    case 1:
        rtime_tmp2 = byte << 4;
        rtime_state = 2;
        break;
    case 2:
        regset[rtime_tmp] = rtime_tmp2 | (byte & 0x0f);
        rtime_state = 0;
        break;
    }
}

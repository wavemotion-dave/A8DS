/*
 * statesav.c - saving the emulator's state to a file
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2006 Atari800 development team (see DOC/CREDITS)
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
#include <string.h>
#include <stdlib.h>
#include "atari.h"
#include "util.h"

/* Value is memory location of data, num is number of type to save */
void SaveUBYTE(const UBYTE *data, int num)
{
}

/* Value is memory location of data, num is number of type to save */
void ReadUBYTE(UBYTE *data, int num)
{
}

/* Value is memory location of data, num is number of type to save */
void SaveUWORD(const UWORD *data, int num)
{
}

/* Value is memory location of data, num is number of type to save */
void ReadUWORD(UWORD *data, int num)
{
}

void SaveINT(const int *data, int num)
{
}

void ReadINT(int *data, int num)
{
}

void SaveFNAME(const char *filename)
{
}

void ReadFNAME(char *filename)
{
}

int SaveAtariState(const char *filename, const char *mode, UBYTE SaveVerbose)
{
    return TRUE;
}

int ReadAtariState(const char *filename, const char *mode)
{
	return TRUE;
}

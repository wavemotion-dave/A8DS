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

#include <stdio.h>

#include "atari.h"
#include "binload.h"
#include "cpu.h"
#include "esc.h"
#include "memory.h"
#include "sio.h"

int BINLOAD_start_binloading = FALSE;
int BINLOAD_loading_basic = 0;
int BINLOAD_slow_xex_loading = FALSE;
FILE *BINLOAD_bin_file = NULL;

/* These variables are for slow XEX loading only. */

/* Number of CPU instructions elapsed since last loaded byte. */
static unsigned int instr_elapsed = 0;
int BINLOAD_wait_active=FALSE;
/* Start and end address of the currently loaded segment. */
static UWORD from = 0;
static UWORD to = 0;
/* Inticates that the next call to loader_cont will overwrite INITAD. */
static int init2e3 = FALSE;
/* Indicates that we are currently not during loading of a segment. */
static int segfinished = TRUE;
int BINLOAD_pause_loading;

/* Read a word from file */
static int read_word(void)
{
    UBYTE buf[2];
    if (fread(buf, 1, 2, BINLOAD_bin_file) != 2) {
        fclose(BINLOAD_bin_file);
        BINLOAD_bin_file = NULL;
        if (BINLOAD_start_binloading) {
            BINLOAD_start_binloading = FALSE;
            return -1;
        }
        CPU_regPC = dGetWord(0x2e0);
        return -1;
    }
    return buf[0] + (buf[1] << 8);
}

/* Start or continue loading */
static void loader_cont(void)
{
    if (BINLOAD_bin_file == NULL)
        return;
    if (BINLOAD_start_binloading) {
        dPutByte(0x244, 0);
        dPutByte(0x09, 1);
    }
    else
        CPU_regS += 2;  /* pop ESC code */

    if (init2e3)
        dPutByte(0x2e3, 0xd7);
    init2e3=FALSE;
    do {
        if((!BINLOAD_wait_active || !BINLOAD_slow_xex_loading) && segfinished){
            int temp;
            do
                temp = read_word();
            while (temp == 0xffff);
            if (temp < 0)
                return;
            from = (UWORD) temp;

            temp = read_word();
            if (temp < 0)
                return;
            to = (UWORD) temp;

            if (BINLOAD_start_binloading) {
                dPutWord(0x2e0, from);
                BINLOAD_start_binloading = FALSE;
            }
            to++;
            segfinished = FALSE;
        }
        do {
            int byte;
            if (BINLOAD_slow_xex_loading) {
                instr_elapsed++;
                if ((instr_elapsed < 300) || BINLOAD_pause_loading) {
                    CPU_regS--;
                    ESC_Add((UWORD) (0x100 + CPU_regS), ESC_BINLOADER_CONT, loader_cont);
                    CPU_regS--;
                    CPU_regPC = CPU_regS + 1 + 0x100;
                    BINLOAD_wait_active = TRUE;
                    return;
                }
                instr_elapsed = 0;
                BINLOAD_wait_active = FALSE;
            }
            byte = fgetc(BINLOAD_bin_file);
            if (byte == EOF) {
                fclose(BINLOAD_bin_file);
                BINLOAD_bin_file = NULL;
                CPU_regPC = dGetWord(0x2e0);
                if (dGetByte(0x2e3) != 0xd7) {
                    /* run INIT routine which RTSes directly to RUN routine */
                    CPU_regPC--;
                    dPutByte(0x0100 + CPU_regS--, CPU_regPC >> 8);      /* high */
                    dPutByte(0x0100 + CPU_regS--, CPU_regPC & 0xff);    /* low */
                    CPU_regPC = dGetWord(0x2e2);
                }
                return;
            }
            PutByte(from, (UBYTE) byte);
            from++;
        } while (from != to);
        segfinished = TRUE;
    } while (dGetByte(0x2e3) == 0xd7);

    CPU_regS--;
    ESC_Add((UWORD) (0x100 + CPU_regS), ESC_BINLOADER_CONT, loader_cont);
    CPU_regS--;
    dPutByte(0x0100 + CPU_regS--, 0x01);    /* high */
    dPutByte(0x0100 + CPU_regS, CPU_regS + 1);  /* low */
    CPU_regS--;
    CPU_regPC = dGetWord(0x2e2);
    CPU_SetC;

    dPutByte(0x0300, 0x31); /* for "Studio Dream" */
    init2e3 = TRUE;
}

/* Fake boot sector to call loader_cont at boot time */
int BINLOAD_LoaderStart(UBYTE *buffer)
{
    buffer[0] = 0x00;   /* ignored */
    buffer[1] = 0x01;   /* one boot sector */
    buffer[2] = 0x00;   /* start at memory location 0x0700 */
    buffer[3] = 0x07;
    buffer[4] = 0x77;   /* reset reboots (0xe477 = Atari OS Coldstart) */
    buffer[5] = 0xe4;
    buffer[6] = 0xf2;   /* ESC */
    buffer[7] = ESC_BINLOADER_CONT;
    ESC_Add(0x706, ESC_BINLOADER_CONT, loader_cont);
    BINLOAD_wait_active = FALSE;
    init2e3 = TRUE;
    segfinished = TRUE;
    return 'C';
}

/* Load BIN file, returns TRUE if ok */
int BINLOAD_Loader(const char *filename)
{
    UBYTE buf[2];
    if (BINLOAD_bin_file != NULL) {     /* close previously open file */
        fclose(BINLOAD_bin_file);
        BINLOAD_bin_file = NULL;
        BINLOAD_loading_basic = 0;
    }
    BINLOAD_bin_file = fopen(filename, "rb");
    if (BINLOAD_bin_file == NULL) { /* open */
        return FALSE;
    }
    /* Avoid "BOOT ERROR" when loading a BASIC program */
    if (SIO_drive_status[0] == SIO_NO_DISK)
        SIO_DisableDrive(1);
    if (fread(buf, 1, 2, BINLOAD_bin_file) == 2) {
        if (buf[0] == 0xff && buf[1] == 0xff) {
            BINLOAD_start_binloading = TRUE; /* force SIO to call BINLOAD_LoaderStart at boot */
            Atari800_Coldstart();             /* reboot */
            return TRUE;
        }
        else if (buf[0] == 0 && buf[1] == 0) {
            BINLOAD_loading_basic = BINLOAD_LOADING_BASIC_SAVED;
            ESC_UpdatePatches();
            Atari800_Coldstart();
            return TRUE;
        }
        else if (buf[0] >= '0' && buf[0] <= '9') {
            BINLOAD_loading_basic = BINLOAD_LOADING_BASIC_LISTED;
            ESC_UpdatePatches();
            Atari800_Coldstart();
            return TRUE;
        }
    }
    fclose(BINLOAD_bin_file);
    BINLOAD_bin_file = NULL;
    return FALSE;
}

/*
 * ATARI.C contains the basic machine settings for the emulator. 
 * This defines how much RAM and whether the machine is compatible
 * with the older stock Atari 800 or the newer XL/XE series (possibly
 * with extended RAM banks).  This routine also sorts out the various
 * supported ROM file types - right now, ATR, ATX and XEX are the only
 * files supported (which will run 99% of everything 8-bit).
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
#include "cpu.h"
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

char disk_filename[DISK_MAX][256];
int  disk_readonly[DISK_MAX] = {true,true,true};

UBYTE file_type        = AFILE_ERROR;

void Warmstart(void) 
{
    if (myConfig.machine_type == MACHINE_800_48K) 
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
    /* handle Option key (disable BASIC in XL/XE) and Start key (boot from cassette) */
    consol_index = 2;
    consol_table[2] = 0x0f;
    if ((myConfig.machine_type >= MACHINE_XLXE_64K) && disable_basic && !BINLOAD_loading_basic)
    {
        /* hold Option during reboot */
        consol_table[2] &= ~CONSOL_OPTION;
    }
    consol_table[1] = consol_table[2];
}

int Atari800_InitialiseMachine(void) 
{
    ESC_ClearAll();
    MEMORY_InitialiseMachine();
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
    if (strstr(filename, ".a52") != 0) return  AFILE_A52;
    if (strstr(filename, ".A52") != 0) return  AFILE_A52;
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
    case AFILE_A52:
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
    //Devices_Initialise(&argc, argv);
    RTIME_Initialise();
    SIO_Initialise (&argc, argv);
    
    strcpy(disk_filename[DISK_XEX], "EMPTY");
    strcpy(disk_filename[DISK_1], "EMPTY");
    strcpy(disk_filename[DISK_2], "EMPTY");    
    
    disk_readonly[DISK_XEX] = true;
    disk_readonly[DISK_1] = true;
    disk_readonly[DISK_2] = true;

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
        SIO_Exit(); /* umount disks, so temporary files are deleted */
    }
    
    //TBD-DSB For now we are keeping the system alive. 
    // Mainly so the user can click into another game 
    // rather than have the emulator just exit.
    bAtariCrash = true;
    
    restart = 1;
    return restart;
}


void Atari800_Frame() 
{
    INPUT_Frame();
    GTIA_Frame();
    ANTIC_Frame(myConfig.skip_frames ? (gTotalAtariFrames & (myConfig.skip_frames==1 ? 0x03:0x01)) : TRUE);  // Skip every 4th frame... or every other frame if we are "aggressive"
    POKEY_Frame();
    
    gTotalAtariFrames++;
}

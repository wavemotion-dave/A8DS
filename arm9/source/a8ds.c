/*
 * A8DS.C contains the main loop and related file selection utilities.
 * This is where the main state machine is located and executes at the
 * TV frequency (60Hz or 50Hz depending on NTSC or PAL).
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
 * be distributed using that same licensing model. See COPYING for the full license.
 */

#include <nds.h>
#include <nds/fifomessages.h>

#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "main.h"
#include "a8ds.h"
#include "CRC32.h"

#include "atari.h"
#include "antic.h"
#include "cartridge.h"
#include "input.h"
#include "esc.h"
#include "memory.h"
#include "rtime.h"
#include "emu/pia.h"

#include "clickNoQuit_wav.h"
#include "keyclick_wav.h"
#include "sio_wav.h"
#include "bgBottom.h"
#include "bgTop.h"
#include "bgFileSel.h"
#include "kbd_XL.h"
#include "kbd_XL2.h"
#include "kbd_XE.h"
#include "kbd_400.h"
#include "kbd_alpha.h"
#include "kbd_starraid.h"
#include "altirra_os.h"
#include "altirra_basic.h"
#include "screenshot.h"
#include "config.h"
#include "highscore.h"
#include "loadsave.h"

FICA_A8 *a8romlist;                         // For reading all the .ATR .XEX .CAR and .ROM files from the SD card
u16 count8bit=0, countfiles=0, ucFicAct=0;  // Counters for all the 8-bit files found on the SD card
u16 gTotalAtariFrames = 0;                  // For FPS counting
int bg0, bg1, bg2, bg3, bg0b, bg1b;         // Background screen "pointers"
u16 emu_state;                              // Emulator State
u16 atari_frames = 0;                       // Number of frames per second (60 for NTSC and 50 for PAL)
u8  bShowKeyboard = false;                  // set to true when the virtual keyboard is showing
u8 bFirstLoad = true;

// ----------------------------------------------------------------------------------
// These are the sound buffer vars which we use to pass along to the ARM7 core.
// This buffer cannot be in .dtcm fast memory because the ARM7 core wouldn't see it.
// ----------------------------------------------------------------------------------
u8 sound_buffer[32]         __attribute__((aligned (2))) = {0};
u16* aptr                   __attribute__((section(".dtcm"))) = (u16*) ((u32)&sound_buffer[0] + 0xA000000);
u16* bptr                   __attribute__((section(".dtcm"))) = (u16*) ((u32)&sound_buffer[2] + 0xA000000);
u16 sound_idx               __attribute__((section(".dtcm"))) = 0;
u8 myPokeyBufIdx            __attribute__((section(".dtcm"))) = 0;
u8 bMute                    __attribute__((section(".dtcm"))) = 0;
u16 sampleExtender[256]     __attribute__((section(".dtcm"))) = {0};

short int screen_slide_x __attribute__((section(".dtcm"))) = 0;
short int screen_slide_y __attribute__((section(".dtcm"))) = 0;

u16 play_sio_sound = 0;

#define  cxBG (myConfig.xOffset<<8)
#define  cyBG (myConfig.yOffset<<8)
#define  xdxBG (((320 / myConfig.xScale) << 8) | (320 % myConfig.xScale))
#define  ydyBG (((256 / myConfig.yScale) << 8) | (256 % myConfig.yScale))

bool bAtariCrash = false;                   // We use this to track any crashes that might occur and give the user a message on screen
char last_boot_file[300] = {0};             // The last booted filename (needed for save states)
char last_disk_filename[2][300] = {"",""};  // The last filename (.ATR or .XEX) we booted (and will be re-booted if RESET pressed) for DISK_1 and DISK_2
char last_working_dir[2][300] = {"",""};    // The last working directory for selecting files 
bool last_read_only[2] = {true, true};      // The last state of whether the user asked to load DISK_1 and DISK_2 as Read-Only or Read-Write
int  last_file_index[2] = {-1, -1};         // The last index of the file chosen by the user on a per drive basis
char file_load_id[12];                      // Some user text to indicate if we are picking a file for XEX/CAR/D1 or D2
u8   file_load_idx = 0;                     // To help us know if we are loading disk 1 or disk 2
bool bLoadAndBoot = true;                   // Force the system to reboot - only for XEX and DISK_1

#define MAX_DEBUG 16
int debug[MAX_DEBUG]={0};                   // Turn on DEBUG_DUMP to output some data to the lower screen... useful for emulator debug: just drop values into debug[] array.
u8 DEBUG_DUMP = 0;                          // Pressing X and then A to select a game will enable the debugger output... it's not much but it's useful!

extern u8 *fake_heap_end;     // current heap start
extern u8 *fake_heap_start;   // current heap end

u8* getHeapStart() {return fake_heap_start;}
u8* getHeapEnd()   {return (u8*)sbrk(0);}
u8* getHeapLimit() {return fake_heap_end;}

int getMemUsed() { // returns the amount of used memory in bytes
   struct mallinfo mi = mallinfo();
   return mi.uordblks;
}

int getMemFree() { // returns the amount of free memory in bytes
   struct mallinfo mi = mallinfo();
   return mi.fordblks + (getHeapLimit() - getHeapEnd());
}

// ---------------------------------------------------------------------------
// Dump Debug Data - write out up to MAX_DEBUG values to the lower screen.
// Useful to help debug the emulator - this goes out approximately once
// per second. Caller just needs to stuff values into the debug[] array.
// ---------------------------------------------------------------------------
ITCM_CODE static void DumpDebugData(void)
{
    if (DEBUG_DUMP)
    {
        static char dbgbuf[33];
        for (int i=0; i<MAX_DEBUG; i++)
        {
            siprintf(dbgbuf, "%02d: %10d  %08X", i, debug[i], debug[i]);
            dsPrintValue(0,3+i,0, dbgbuf);
        }

        struct mallinfo mi = mallinfo();
        siprintf(dbgbuf, "DYNAMIC MEM USED: %d ", mi.uordblks);
        dsPrintValue(0,22,0, dbgbuf);
        siprintf(dbgbuf, "FREE HEAP MEMORY: %d ", getMemFree());
        dsPrintValue(0,23,0, dbgbuf);
    }
}


// ---------------------------------------------------------------------------
// Called when the SIO driver indicates disk activity so we can show a small
// pattern on the top line of the bottom display - the user waits while loading.
// ---------------------------------------------------------------------------
static u8 actidx=0;
#ifndef NEW_CYCLE_EXACT
ITCM_CODE
#endif
void dsShowDiskActivity(int drive)
{
    static char activity[8] = {'*','+','*','*','+','+','+',' '};
    char buf[5];

    // ----------------------------------------------------------------
    // If we are using normal disk speed access, we disable the SOUNDR
    // register so that the OS ROM will not make chirp noise. We'll do
    // the sound effects ourselves and it will sound cleaner...
    // ----------------------------------------------------------------
    if (myConfig.disk_speedup == 0)
    {
        dPutByte(0x0041, 0x00);  // SOUNDR ... disable I/O Sound in the OS ROM
    }

    buf[0] = 'D';
    buf[1] = '1'+drive;
    buf[2] = activity[++actidx & 0x7];
    buf[3] = 0;
    dsPrintValue(3,0,0, buf);

    if (play_sio_sound < (myConfig.disk_speedup ? 25 : 75)) play_sio_sound += 25;
}


// ---------------------------------------------------------------------------
// This is called very frequently (about 16,000 times per second) to fill the
// pipeline of sound values from the pokey buffer into the Nintendo DS sound
// buffer which will be processed in the background by the ARM 7 processor.
// ---------------------------------------------------------------------------
ITCM_CODE void VsoundHandler(void)
{
    extern unsigned char pokey_buffer[];
    extern u16 pokeyBufIdx;

    if (bMute) *bptr = *aptr;

    // If there is a fresh sample...
    else if (myPokeyBufIdx != pokeyBufIdx)
    {
        u16 sample = sampleExtender[pokey_buffer[myPokeyBufIdx++]];
        *aptr = sample;
        *bptr = sample;
    }
}

// ---------------------------------------------------------------------------
// We have 2 palettes - one for NTSC and one for PAL.
// ---------------------------------------------------------------------------
#define PALETTE_SIZE 768
const byte palette_NTSC[PALETTE_SIZE] =
{
    0x00, 0x00, 0x00, 0x25, 0x25, 0x25, 0x34, 0x34, 0x34, 0x4F, 0x4F, 0x4F,
    0x5B, 0x5B, 0x5B, 0x69, 0x69, 0x69, 0x7B, 0x7B, 0x7B, 0x8A, 0x8A, 0x8A,
    0xA7, 0xA7, 0xA7, 0xB9, 0xB9, 0xB9, 0xC5, 0xC5, 0xC5, 0xD0, 0xD0, 0xD0,
    0xD7, 0xD7, 0xD7, 0xE1, 0xE1, 0xE1, 0xF4, 0xF4, 0xF4, 0xFF, 0xFF, 0xFF,
    0x4C, 0x32, 0x00, 0x62, 0x3A, 0x00, 0x7B, 0x4A, 0x00, 0x9A, 0x60, 0x00,
    0xB5, 0x74, 0x00, 0xCC, 0x85, 0x00, 0xE7, 0x9E, 0x08, 0xF7, 0xAF, 0x10,
    0xFF, 0xC3, 0x18, 0xFF, 0xD0, 0x20, 0xFF, 0xD8, 0x28, 0xFF, 0xDF, 0x30,
    0xFF, 0xE6, 0x3B, 0xFF, 0xF4, 0x40, 0xFF, 0xFA, 0x4B, 0xFF, 0xFF, 0x50,
    0x99, 0x25, 0x00, 0xAA, 0x25, 0x00, 0xB4, 0x25, 0x00, 0xD3, 0x30, 0x00,
    0xDD, 0x48, 0x02, 0xE2, 0x50, 0x09, 0xF4, 0x67, 0x00, 0xF4, 0x75, 0x10,
    0xFF, 0x9E, 0x10, 0xFF, 0xAC, 0x20, 0xFF, 0xBA, 0x3A, 0xFF, 0xBF, 0x50,
    0xFF, 0xC6, 0x6D, 0xFF, 0xD5, 0x80, 0xFF, 0xE4, 0x90, 0xFF, 0xE6, 0x99,
    0x98, 0x0C, 0x0C, 0x99, 0x0C, 0x0C, 0xC2, 0x13, 0x00, 0xD3, 0x13, 0x00,
    0xE2, 0x35, 0x00, 0xE3, 0x40, 0x00, 0xE4, 0x40, 0x20, 0xE5, 0x52, 0x30,
    0xFD, 0x78, 0x54, 0xFF, 0x8A, 0x6A, 0xFF, 0x98, 0x7C, 0xFF, 0xA4, 0x8B,
    0xFF, 0xB3, 0x9E, 0xFF, 0xC2, 0xB2, 0xFF, 0xD0, 0xBA, 0xFF, 0xD7, 0xC0,
    0x99, 0x00, 0x00, 0xA9, 0x00, 0x00, 0xC2, 0x04, 0x00, 0xD3, 0x04, 0x00,
    0xDA, 0x04, 0x00, 0xDB, 0x08, 0x00, 0xE4, 0x20, 0x20, 0xF6, 0x40, 0x40,
    0xFB, 0x70, 0x70, 0xFB, 0x7E, 0x7E, 0xFB, 0x8F, 0x8F, 0xFF, 0x9F, 0x9F,
    0xFF, 0xAB, 0xAB, 0xFF, 0xB9, 0xB9, 0xFF, 0xC9, 0xC9, 0xFF, 0xCF, 0xCF,
    0x7E, 0x00, 0x50, 0x80, 0x00, 0x50, 0x80, 0x00, 0x5F, 0x95, 0x0B, 0x74,
    0xAA, 0x22, 0x88, 0xBB, 0x2F, 0x9A, 0xCE, 0x3F, 0xAD, 0xD7, 0x5A, 0xB6,
    0xE4, 0x67, 0xC3, 0xEF, 0x72, 0xCE, 0xFB, 0x7E, 0xDA, 0xFF, 0x8D, 0xE1,
    0xFF, 0x9D, 0xE5, 0xFF, 0xA5, 0xE7, 0xFF, 0xAF, 0xEA, 0xFF, 0xB8, 0xEC,
    0x48, 0x00, 0x6C, 0x5C, 0x04, 0x88, 0x65, 0x0D, 0x90, 0x7B, 0x23, 0xA7,
    0x93, 0x3B, 0xBF, 0x9D, 0x45, 0xC9, 0xA7, 0x4F, 0xD3, 0xB2, 0x5A, 0xDE,
    0xBD, 0x65, 0xE9, 0xC5, 0x6D, 0xF1, 0xCE, 0x76, 0xFA, 0xD5, 0x83, 0xFF,
    0xDA, 0x90, 0xFF, 0xDE, 0x9C, 0xFF, 0xE2, 0xA9, 0xFF, 0xE6, 0xB6, 0xFF,
    0x1B, 0x00, 0x70, 0x22, 0x1B, 0x8D, 0x37, 0x30, 0xA2, 0x48, 0x41, 0xB3,
    0x59, 0x52, 0xC4, 0x63, 0x5C, 0xCE, 0x6F, 0x68, 0xDA, 0x7D, 0x76, 0xE8,
    0x87, 0x80, 0xF8, 0x93, 0x8C, 0xFF, 0x9D, 0x97, 0xFF, 0xA8, 0xA3, 0xFF,
    0xB3, 0xAF, 0xFF, 0xBC, 0xB8, 0xFF, 0xC4, 0xC1, 0xFF, 0xDA, 0xD1, 0xFF,
    0x00, 0x0D, 0x7F, 0x00, 0x12, 0xA7, 0x00, 0x18, 0xC0, 0x0A, 0x2B, 0xD1,
    0x1B, 0x4A, 0xE3, 0x2F, 0x58, 0xF0, 0x37, 0x68, 0xFF, 0x49, 0x79, 0xFF,
    0x5B, 0x85, 0xFF, 0x6D, 0x96, 0xFF, 0x7F, 0xA3, 0xFF, 0x8C, 0xAD, 0xFF,
    0x96, 0xB4, 0xFF, 0xA8, 0xC0, 0xFF, 0xB7, 0xCB, 0xFF, 0xC6, 0xD6, 0xFF,
    0x00, 0x29, 0x5A, 0x00, 0x38, 0x76, 0x00, 0x48, 0x92, 0x00, 0x5C, 0xAC,
    0x00, 0x71, 0xC6, 0x00, 0x86, 0xD0, 0x0A, 0x9B, 0xDF, 0x1A, 0xA8, 0xEC,
    0x2B, 0xB6, 0xFF, 0x3F, 0xC2, 0xFF, 0x45, 0xCB, 0xFF, 0x59, 0xD3, 0xFF,
    0x7F, 0xDA, 0xFF, 0x8F, 0xDE, 0xFF, 0xA0, 0xE2, 0xFF, 0xB0, 0xEB, 0xFF,
    0x00, 0x4A, 0x00, 0x00, 0x4C, 0x00, 0x00, 0x6A, 0x20, 0x50, 0x8E, 0x79,
    0x40, 0x99, 0x99, 0x00, 0x9C, 0xAA, 0x00, 0xA1, 0xBB, 0x01, 0xA4, 0xCC,
    0x03, 0xA5, 0xD7, 0x05, 0xDA, 0xE2, 0x18, 0xE5, 0xFF, 0x34, 0xEA, 0xFF,
    0x49, 0xEF, 0xFF, 0x66, 0xF2, 0xFF, 0x84, 0xF4, 0xFF, 0x9E, 0xF9, 0xFF,
    0x00, 0x4A, 0x00, 0x00, 0x5D, 0x00, 0x00, 0x70, 0x00, 0x00, 0x83, 0x00,
    0x00, 0x95, 0x00, 0x00, 0xAB, 0x00, 0x07, 0xBD, 0x07, 0x0A, 0xD0, 0x0A,
    0x1A, 0xD5, 0x40, 0x5A, 0xF1, 0x77, 0x82, 0xEF, 0xA7, 0x84, 0xED, 0xD1,
    0x89, 0xFF, 0xED, 0x7D, 0xFF, 0xFF, 0x93, 0xFF, 0xFF, 0x9B, 0xFF, 0xFF,
    0x22, 0x4A, 0x03, 0x27, 0x53, 0x04, 0x30, 0x64, 0x05, 0x3C, 0x77, 0x0C,
    0x45, 0x8C, 0x11, 0x5A, 0xA5, 0x13, 0x1B, 0xD2, 0x09, 0x1F, 0xDD, 0x00,
    0x3D, 0xCD, 0x2D, 0x3D, 0xCD, 0x30, 0x58, 0xCC, 0x40, 0x60, 0xD3, 0x50,
    0xA2, 0xEC, 0x55, 0xB3, 0xF2, 0x4A, 0xBB, 0xF6, 0x5D, 0xC4, 0xF8, 0x70,
    0x2E, 0x3F, 0x0C, 0x36, 0x4A, 0x0F, 0x40, 0x56, 0x15, 0x46, 0x5F, 0x17,
    0x57, 0x77, 0x1A, 0x65, 0x85, 0x1C, 0x74, 0x93, 0x1D, 0x8F, 0xA5, 0x25,
    0xAD, 0xB7, 0x2C, 0xBC, 0xC7, 0x30, 0xC9, 0xD5, 0x33, 0xD4, 0xE0, 0x3B,
    0xE0, 0xEC, 0x42, 0xEA, 0xF6, 0x45, 0xF0, 0xFD, 0x47, 0xF4, 0xFF, 0x6F,
    0x55, 0x24, 0x00, 0x5A, 0x2C, 0x00, 0x6C, 0x3B, 0x00, 0x79, 0x4B, 0x00,
    0xB9, 0x75, 0x00, 0xBB, 0x85, 0x00, 0xC1, 0xA1, 0x20, 0xD0, 0xB0, 0x2F,
    0xDE, 0xBE, 0x3F, 0xE6, 0xC6, 0x45, 0xED, 0xCD, 0x57, 0xF5, 0xDB, 0x62,
    0xFB, 0xE5, 0x69, 0xFC, 0xEE, 0x6F, 0xFD, 0xF3, 0x77, 0xFD, 0xF3, 0x7F,
    0x5C, 0x27, 0x00, 0x5C, 0x2F, 0x00, 0x71, 0x3B, 0x00, 0x7B, 0x48, 0x00,
    0xB9, 0x68, 0x20, 0xBB, 0x72, 0x20, 0xC5, 0x86, 0x29, 0xD7, 0x96, 0x33,
    0xE6, 0xA4, 0x40, 0xF4, 0xB1, 0x4B, 0xFD, 0xC1, 0x58, 0xFF, 0xCC, 0x55,
    0xFF, 0xD4, 0x61, 0xFF, 0xDD, 0x69, 0xFF, 0xE6, 0x79, 0xFF, 0xEA, 0x98
};

const byte palette_PAL[PALETTE_SIZE] =
{
    0x00, 0x00, 0x00, 0x11, 0x11, 0x11, 0x25, 0x25, 0x25, 0x35, 0x35, 0x35,
    0x46, 0x46, 0x46, 0x57, 0x57, 0x57, 0x6B, 0x6B, 0x6B, 0x7C, 0x7C, 0x7C,
    0x83, 0x83, 0x83, 0x94, 0x94, 0x94, 0xA8, 0xA8, 0xA8, 0xB9, 0xB9, 0xB9,
    0xCA, 0xCA, 0xCA, 0xDA, 0xDA, 0xDA, 0xEE, 0xEE, 0xEE, 0xFF, 0xFF, 0xFF,
    0x3C, 0x00, 0x00, 0x4C, 0x07, 0x00, 0x60, 0x1B, 0x00, 0x71, 0x2C, 0x00,
    0x82, 0x3C, 0x00, 0x93, 0x4D, 0x00, 0xA6, 0x61, 0x00, 0xB7, 0x72, 0x11,
    0xBF, 0x7A, 0x19, 0xD0, 0x8B, 0x2A, 0xE4, 0x9E, 0x3E, 0xF4, 0xAF, 0x4F,
    0xFF, 0xC0, 0x5F, 0xFF, 0xD1, 0x70, 0xFF, 0xE4, 0x84, 0xFF, 0xF5, 0x95,
    0x4B, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x70, 0x0B, 0x00, 0x81, 0x1C, 0x00,
    0x91, 0x2D, 0x03, 0xA2, 0x3E, 0x13, 0xB6, 0x51, 0x27, 0xC7, 0x62, 0x38,
    0xCF, 0x6A, 0x40, 0xE0, 0x7B, 0x51, 0xF3, 0x8F, 0x65, 0xFF, 0xA0, 0x75,
    0xFF, 0xB0, 0x86, 0xFF, 0xC1, 0x97, 0xFF, 0xD5, 0xAB, 0xFF, 0xE6, 0xBB,
    0x50, 0x00, 0x15, 0x61, 0x00, 0x26, 0x74, 0x00, 0x3A, 0x85, 0x09, 0x4A,
    0x96, 0x19, 0x5B, 0xA7, 0x2A, 0x6C, 0xBB, 0x3E, 0x80, 0xCB, 0x4F, 0x91,
    0xD3, 0x57, 0x98, 0xE4, 0x68, 0xA9, 0xF8, 0x7B, 0xBD, 0xFF, 0x8C, 0xCE,
    0xFF, 0x9D, 0xDF, 0xFF, 0xAE, 0xEF, 0xFF, 0xC1, 0xFF, 0xFF, 0xD2, 0xFF,
    0x3D, 0x00, 0x67, 0x4E, 0x00, 0x78, 0x62, 0x00, 0x8C, 0x73, 0x02, 0x9D,
    0x83, 0x13, 0xAE, 0x94, 0x24, 0xBE, 0xA8, 0x38, 0xD2, 0xB9, 0x48, 0xE3,
    0xC1, 0x50, 0xEB, 0xD1, 0x61, 0xFC, 0xE5, 0x75, 0xFF, 0xF6, 0x86, 0xFF,
    0xFF, 0x96, 0xFF, 0xFF, 0xA7, 0xFF, 0xFF, 0xBB, 0xFF, 0xFF, 0xCC, 0xFF,
    0x28, 0x00, 0x84, 0x39, 0x00, 0x95, 0x4C, 0x00, 0xA9, 0x5D, 0x07, 0xBA,
    0x6E, 0x18, 0xCB, 0x7F, 0x29, 0xDB, 0x93, 0x3D, 0xEF, 0xA3, 0x4E, 0xFF,
    0xAB, 0x55, 0xFF, 0xBC, 0x66, 0xFF, 0xD0, 0x7A, 0xFF, 0xE1, 0x8B, 0xFF,
    0xF1, 0x9C, 0xFF, 0xFF, 0xAC, 0xFF, 0xFF, 0xC0, 0xFF, 0xFF, 0xD1, 0xFF,
    0x0F, 0x00, 0x94, 0x1F, 0x00, 0xA5, 0x33, 0x00, 0xB9, 0x44, 0x11, 0xC9,
    0x55, 0x22, 0xDA, 0x65, 0x33, 0xEB, 0x79, 0x47, 0xFF, 0x8A, 0x57, 0xFF,
    0x92, 0x5F, 0xFF, 0xA3, 0x70, 0xFF, 0xB7, 0x84, 0xFF, 0xC7, 0x95, 0xFF,
    0xD8, 0xA6, 0xFF, 0xE9, 0xB6, 0xFF, 0xFD, 0xCA, 0xFF, 0xFF, 0xDB, 0xFF,
    0x00, 0x00, 0x86, 0x00, 0x0A, 0x97, 0x00, 0x1E, 0xAB, 0x10, 0x2F, 0xBC,
    0x20, 0x3F, 0xCC, 0x31, 0x50, 0xDD, 0x45, 0x64, 0xF1, 0x56, 0x75, 0xFF,
    0x5E, 0x7D, 0xFF, 0x6E, 0x8E, 0xFF, 0x82, 0xA1, 0xFF, 0x93, 0xB2, 0xFF,
    0xA4, 0xC3, 0xFF, 0xB5, 0xD4, 0xFF, 0xC8, 0xE7, 0xFF, 0xD9, 0xF8, 0xFF,
    0x00, 0x0A, 0x6A, 0x00, 0x1B, 0x7B, 0x00, 0x2E, 0x8F, 0x00, 0x3F, 0xA0,
    0x0B, 0x50, 0xB0, 0x1B, 0x61, 0xC1, 0x2F, 0x74, 0xD5, 0x40, 0x85, 0xE6,
    0x48, 0x8D, 0xEE, 0x59, 0x9E, 0xFF, 0x6C, 0xB2, 0xFF, 0x7D, 0xC3, 0xFF,
    0x8E, 0xD3, 0xFF, 0x9F, 0xE4, 0xFF, 0xB3, 0xF8, 0xFF, 0xC3, 0xFF, 0xFF,
    0x00, 0x19, 0x44, 0x00, 0x2A, 0x54, 0x00, 0x3E, 0x68, 0x00, 0x4F, 0x79,
    0x00, 0x5F, 0x8A, 0x0C, 0x70, 0x9A, 0x1F, 0x84, 0xAE, 0x30, 0x95, 0xBF,
    0x38, 0x9D, 0xC7, 0x49, 0xAE, 0xD8, 0x5D, 0xC1, 0xEC, 0x6E, 0xD2, 0xFC,
    0x7E, 0xE3, 0xFF, 0x8F, 0xF4, 0xFF, 0xA3, 0xFF, 0xFF, 0xB4, 0xFF, 0xFF,
    0x00, 0x2D, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x51, 0x10, 0x00, 0x62, 0x20,
    0x00, 0x73, 0x31, 0x07, 0x84, 0x42, 0x1B, 0x97, 0x56, 0x2C, 0xA8, 0x67,
    0x34, 0xB0, 0x6E, 0x44, 0xC1, 0x7F, 0x58, 0xD5, 0x93, 0x69, 0xE6, 0xA4,
    0x7A, 0xF6, 0xB5, 0x8B, 0xFF, 0xC5, 0x9E, 0xFF, 0xD9, 0xAF, 0xFF, 0xEA,
    0x00, 0x2E, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x53, 0x00, 0x0E, 0x63, 0x00,
    0x1E, 0x74, 0x00, 0x2F, 0x85, 0x00, 0x43, 0x99, 0x00, 0x54, 0xAA, 0x00,
    0x5C, 0xB1, 0x00, 0x6C, 0xC2, 0x10, 0x80, 0xD6, 0x24, 0x91, 0xE7, 0x34,
    0xA2, 0xF8, 0x45, 0xB3, 0xFF, 0x56, 0xC6, 0xFF, 0x6A, 0xD7, 0xFF, 0x7B,
    0x00, 0x24, 0x00, 0x02, 0x35, 0x00, 0x16, 0x49, 0x00, 0x27, 0x59, 0x00,
    0x38, 0x6A, 0x00, 0x48, 0x7B, 0x00, 0x5C, 0x8F, 0x00, 0x6D, 0xA0, 0x00,
    0x75, 0xA8, 0x00, 0x86, 0xB8, 0x00, 0x9A, 0xCC, 0x14, 0xAA, 0xDD, 0x25,
    0xBB, 0xEE, 0x36, 0xCC, 0xFF, 0x46, 0xE0, 0xFF, 0x5A, 0xF0, 0xFF, 0x6B,
    0x0C, 0x17, 0x00, 0x1D, 0x27, 0x00, 0x31, 0x3B, 0x00, 0x42, 0x4C, 0x00,
    0x52, 0x5D, 0x00, 0x63, 0x6E, 0x00, 0x77, 0x81, 0x00, 0x88, 0x92, 0x00,
    0x90, 0x9A, 0x00, 0xA1, 0xAB, 0x00, 0xB4, 0xBF, 0x13, 0xC5, 0xCF, 0x24,
    0xD6, 0xE0, 0x35, 0xE7, 0xF1, 0x46, 0xFB, 0xFF, 0x5A, 0xFF, 0xFF, 0x6A,
    0x26, 0x07, 0x00, 0x37, 0x18, 0x00, 0x4A, 0x2B, 0x00, 0x5B, 0x3C, 0x00,
    0x6C, 0x4D, 0x00, 0x7D, 0x5E, 0x00, 0x91, 0x71, 0x00, 0xA1, 0x82, 0x00,
    0xA9, 0x8A, 0x00, 0xBA, 0x9B, 0x0E, 0xCE, 0xAF, 0x22, 0xDF, 0xC0, 0x33,
    0xEF, 0xD0, 0x43, 0xFF, 0xE1, 0x54, 0xFF, 0xF5, 0x68, 0xFF, 0xFF, 0x79,
    0x3C, 0x00, 0x00, 0x4C, 0x07, 0x00, 0x60, 0x1B, 0x00, 0x71, 0x2C, 0x00,
    0x82, 0x3C, 0x00, 0x93, 0x4D, 0x00, 0xA6, 0x61, 0x00, 0xB7, 0x72, 0x11,
    0xBF, 0x7A, 0x19, 0xD0, 0x8B, 0x2A, 0xE4, 0x9E, 0x3E, 0xF4, 0xAF, 0x4F,
    0xFF, 0xC0, 0x5F, 0xFF, 0xD1, 0x70, 0xFF, 0xE4, 0x84, 0xFF, 0xF5, 0x95
};

// --------------------------------------------------------------------
// Set Palette A (NTSC) or B (PAL) from the big tables above...
// --------------------------------------------------------------------
void dsSetAtariPalette(void)
{
    unsigned int index;
    unsigned short r;
    unsigned short g;
    unsigned short b;

    // Init palette
    for(index = 0; index < 256; index++)
    {
      if (myConfig.tv_type == TV_NTSC)
      {
        r = palette_NTSC[(index * 3) + 0];
        g = palette_NTSC[(index * 3) + 1];
        b = palette_NTSC[(index * 3) + 2];
      }
      else
      {
        r = palette_PAL[(index * 3) + 0];
        g = palette_PAL[(index * 3) + 1];
        b = palette_PAL[(index * 3) + 2];
      }
      BG_PALETTE[index] = RGB8(r, g, b);
    }
}

// ----------------------------------------------------
// Color fading effect used for the intro screen.
// ----------------------------------------------------
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait)
{
    unsigned short ucFade;
    unsigned char ucBcl;

    // Fade-out to black...
    if (ucScr & 0x01) REG_BLDCNT=ucBG;
    if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
    if (ucSens == 1)
    {
        for(ucFade=0;ucFade<valEnd;ucFade++)
        {
            if (ucScr & 0x01) REG_BLDY=ucFade;
            if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
            for (ucBcl=0;ucBcl<uWait;ucBcl++)
            {
                swiWaitForVBlank();
            }
        }
    }
    else
    {
        for(ucFade=16;ucFade>valEnd;ucFade--)
        {
            if (ucScr & 0x01) REG_BLDY=ucFade;
            if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
            for (ucBcl=0;ucBcl<uWait;ucBcl++)
            {
                swiWaitForVBlank();
            }
        }
    }
}

// -----------------------------------------------------------------------
// We use the jitter arrays to dither the pixel output slighty
// on screen so that the eye is tricked into seeing full resolution.
// This is needed because the Atari horizontal resolution can be
// up to 320 pixels but the DS only has 256. By doing this, we
// see a better rendition of letters and numbers which othewise
// will have pixels not showing. It's far from perfect - but workable.
// -----------------------------------------------------------------------
static int sIndex __attribute__((section(".dtcm")))= 0;
static u8 jitter[][4] __attribute__((section(".dtcm"))) =
{
    {0x00, 0x00,
     0x00, 0x00},

    {0x00, 0x00,
     0x40, 0x00},

    {0x00, 0x00,
     0x80, 0x00},
};

UBYTE dampen_slide_y __attribute__((section(".dtcm")))= 0;
UBYTE dampen_slide_x __attribute__((section(".dtcm")))= 0;

u8 bScreenZoom       __attribute__((section(".dtcm")))= 0;
s16 bZoomX           __attribute__((section(".dtcm")))= 0;
s16 bZoomY           __attribute__((section(".dtcm")))= 0;
ITCM_CODE void vblankIntr()
{
    if (bScreenZoom)
    {
        REG_BG2PA = REG_BG3PA = 0x0100; // Zoom is 1:1 (320x256)
        REG_BG2PD = REG_BG3PD = 0x0100; // Zoom is 1:1 (320x256)

        REG_BG3X = REG_BG2X = cxBG + (bZoomX << 8);
        REG_BG3Y = REG_BG2Y = cyBG + (bZoomY << 8);
    }
    else
    {
        REG_BG2PA = REG_BG3PA = xdxBG;
        REG_BG2PD = REG_BG3PD = ydyBG;

        REG_BG3X = REG_BG2X = cxBG+jitter[myConfig.blending][sIndex++]+(screen_slide_x<<8);
        REG_BG3Y = REG_BG2Y = cyBG+jitter[myConfig.blending][sIndex++]+(screen_slide_y<<8);

        sIndex = sIndex & 0x03;

        if (sIndex == 0)
        {
            if (dampen_slide_y == 0)
            {
                if (screen_slide_y < 0) screen_slide_y++;
                else if (screen_slide_y > 0) screen_slide_y--;
            } else dampen_slide_y--;

            if (dampen_slide_x == 0)
            {
                if (screen_slide_x < 0) screen_slide_x++;
                else if (screen_slide_x > 0) screen_slide_x--;
            } else dampen_slide_x--;
        }
    }
}

/*
A: ARM9 0x06800000 - 0x0681FFFF (128KB)
B: ARM9 0x06820000 - 0x0683FFFF (128KB)
C: ARM9 0x06840000 - 0x0685FFFF (128KB)
D: ARM9 0x06860000 - 0x0687FFFF (128KB)
E: ARM9 0x06880000 - 0x0688FFFF ( 64KB)
F: ARM9 0x06890000 - 0x06893FFF ( 16KB)
G: ARM9 0x06894000 - 0x06897FFF ( 16KB)
H: ARM9 0x06898000 - 0x0689FFFF ( 32KB)
I: ARM9 0x068A0000 - 0x068A3FFF ( 16KB)
*/
void dsInitScreenMain(void)
{
    // Init vbl and hbl func
    SetYtrigger(190); //trigger 2 lines before vsync
    irqSet(IRQ_VBLANK, vblankIntr);
    irqEnable(IRQ_VBLANK);
    vramSetBankA(VRAM_A_MAIN_BG);             // This is the main Emulation screen - will be Alpha Blended with VRAM_B
    vramSetBankB(VRAM_B_MAIN_BG);             // This is the main Emulation screen - will be Alpha Blended with VRAM_A
    vramSetBankC(VRAM_C_SUB_BG);              // This is the Sub-Screen (touch screen) display (2 layers)
    vramSetBankD(VRAM_D_LCD );                // Not using this for video but need the 128K of VRAM to capture screenshots (DCAP)
    vramSetBankE(VRAM_E_LCD );                // Not using this for video but  64K of faster RAM always useful!  Mapped at 0x06880000 (unused)
    vramSetBankF(VRAM_F_LCD );                // Not using this for video but  16K of faster RAM always useful!  Mapped at 0x06890000 (unused)
    vramSetBankG(VRAM_G_LCD );                // Not using this for video but  16K of faster RAM always useful!  Mapped at 0x06894000 (unused)
    vramSetBankH(VRAM_H_LCD );                // Not using this for video but  32K of faster RAM always useful!  Mapped at 0x06898000 (16K for Shadow RAM under XL OS and 16K for Pristine OS)
    vramSetBankI(VRAM_I_LCD );                // Not using this for video but  16K of faster RAM always useful!  Mapped at 0x068A0000 (used for GTIA lookup table)

    ReadGameSettings();
}

// --------------------------------------------------
// Enables TIMER0 (our main timer) to start running.
// This timer runs at 32,728.5 ticks = 1 second.
// --------------------------------------------------
void dsInitTimer(void)
{
    TIMER0_DATA=0;
    TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
}

// --------------------------------------------------
// This routine is called whenever we want to enable
// the emulation on the top screen. It sets up for
// an internal 512x512 buffer at 8 bits of color per
// pixel. This is sufficient for any Atari rendering.
// The actual DS display is 256x192 and so there must
// be some scaling/offset handling which is handled
// below as well.
// --------------------------------------------------
void dsShowScreenEmu(void)
{
    // Change vram
    if (myConfig.alphaBlend)
    {
        videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE);
        bg2 = bgInit(2, BgType_Bmp8, BgSize_B8_512x512, 0,0);
        bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_512x512, 0,0);

        REG_BLDY=0;

        // Trying a 25/75 Alpha Blend...
        REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG2 | BLEND_DST_BG3;
        REG_BLDALPHA = (0x9 << 8) | 0xF; // 25% / 75%

        REG_BG3PB = 0;
        REG_BG3PC = 0;

        REG_BG3X = cxBG;
        REG_BG3Y = cyBG;
        REG_BG3PA = xdxBG;
        REG_BG3PD = ydyBG;
    }
    else
    {
        videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
        bg2 = bgInit(2, BgType_Bmp8, BgSize_B8_512x512, 0,0);

        REG_BLDY=0;
        REG_BLDCNT = 0;
        REG_BLDALPHA = 0;

        REG_BG3PB = 0;
        REG_BG3PC = 0;

        REG_BG3X = 0;
        REG_BG3Y = 0;
        REG_BG3PA = 0;
        REG_BG3PD = 0;
    }

    REG_BG2PB = 0;
    REG_BG2PC = 0;

    REG_BG2X = cxBG;
    REG_BG2Y = cyBG;
    REG_BG2PA = xdxBG;
    REG_BG2PD = ydyBG;

}

// --------------------------------------------------------------------
// The main screen is the bottom display (under the emulator screen).
// We use a smaller 256x256 buffer and some of the off-screen is used
// to store some sprites for the A-Z,0-9 alphabet so we can display
// lots of text superimposed over the background image.
// --------------------------------------------------------------------
void dsShowScreenMain(void)
{
    // Init BG mode for 16 bits colors
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
    videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE);

    bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
    bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
    bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 30,0);
    bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);

    decompress(bgTopTiles, bgGetGfxPtr(bg0), LZ77Vram);
    decompress(bgTopMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
    dmaCopy((void *) bgTopPal,(u16*) BG_PALETTE,256*2);

    decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

    REG_BLDCNT=0; REG_BLDY=0;
    REG_BLDCNT_SUB=0; REG_BLDY_SUB=0;

    swiWaitForVBlank();

    dsShowRomInfo();
}

// -------------------------------------------------------------------
// Turns off timer 2 to stop any sound processing... used when we
// quit the emulator.
// -------------------------------------------------------------------
void dsFreeEmu(void)
{
    // Stop timer of sound
    TIMER2_CR=0; irqDisable(IRQ_TIMER2);
}


// -------------------------------------------------------------------
// At the start of the world, we read in the three possible ROM images
// that the emulator will use. These are:
//      - 16k XL/XE ROM BIOS (either Altirra-XL or AtariXL.rom)
//      - 12k Atari 800 ROM BIOS (either Altirra-800 or AtariOSB.rom)
//      - 8k  BASIC Cart (either Altirra BASIC or AtariBAS.rom)
// If the Atari external ROMs are not found, the built-in Altirra
// ones are used instead.
// -------------------------------------------------------------------
UBYTE ROM_atarios_xl[0x4000];
UBYTE ROM_atarios_b[0x2800];
UBYTE ROM_basic[0x2000];

void load_os(void)
{
    // -------------------------------------------
    // Read XL/XE ROM from SD card...
    // -------------------------------------------
    FILE *romfile = fopen("atarixl.rom", "rb");
    if (romfile == NULL) romfile = fopen("/roms/bios/atarixl.rom", "rb");
    if (romfile == NULL) romfile = fopen("/data/bios/atarixl.rom", "rb");

    if (romfile == NULL)
    {
        // If we can't find the atari OS, we force the Altirra XL bios in...
        memcpy(ROM_atarios_xl, ROM_altirraos_xl, 0x4000);
        bAtariOS = false;
        myConfig.os_type = OS_ALTIRRA_XL;    // Default is built-in OS from Alitirra if XL rom not found
    }
    else
    {
        fread(ROM_atarios_xl, 0x4000, 1, romfile);
        fclose(romfile);
        bAtariOS = true;
        // os_type was already set prior to this call
    }

    // -------------------------------------------
    // Read A800 older ROM from SD card...
    // -------------------------------------------
    romfile = fopen("atariosb.rom", "rb");
    if (romfile == NULL) romfile = fopen("/roms/bios/atariosb.rom", "rb");
    if (romfile == NULL) romfile = fopen("/data/bios/atariosb.rom", "rb");
    if (romfile == NULL)
    {
        // If we can't find the atari OSB, we force the Altirra 800 bios in
        memcpy(ROM_atarios_b, ROM_altirraos_800, 0x2800);
        bAtariOSB = false;
    }
    else
    {
        fread(ROM_atarios_b, 0x2800, 1, romfile);
        fclose(romfile);
        bAtariOSB = true;
    }

    // -------------------------------------------
    // Read Atari BASIC (rev C) from SD card...
    // -------------------------------------------
    FILE *basfile = fopen("ataribas.rom", "rb");
    if (basfile == NULL) basfile = fopen("/roms/bios/ataribas.rom", "rb");
    if (basfile == NULL) basfile = fopen("/data/bios/ataribas.rom", "rb");
    if (basfile == NULL)
    {
        // If we can't find the atari Basic, we force the Altirra in...
        memcpy(ROM_basic, ROM_altirra_basic, 0x2000);
        bAtariBASIC = false;
    }
    else
    {
        fread(ROM_basic, 0x2000, 1, basfile);
        fclose(basfile);
        bAtariBASIC = true;
    }

    return;
} /* end load_os() */


// ----------------------------------------------------------------------------
// Called on every reset/cold start to load in the proper OS into the
// right area of memory (upper 16K bank). The XL/XE has some cool handling
// that allows writes to PortB to turn off the OS to expose "RAM under the OS"
// so we need to be able to shift the OS in and out of memory. See the PortB
// handling for more details on how this works...
// ----------------------------------------------------------------------------
void install_os(void)
{
    memset(debug, 0x00, sizeof(debug));

    // Otherwise we either use the Atari OS or the Altirra based on user choice...
    if (myConfig.os_type == OS_ALTIRRA_XL)
    {
        memcpy(atari_os, ROM_altirraos_xl, 0x4000);
        machine_type = MACHINE_XLXE;
    }
    else if (myConfig.os_type == OS_ALTIRRA_800)
    {
        memcpy(atari_os+0x1800, ROM_altirraos_800, 0x2800);
        machine_type = MACHINE_OSB;
    }
    else if (myConfig.os_type == OS_ATARI_OSB)
    {
        memcpy(atari_os+0x1800, ROM_atarios_b, 0x2800);
        machine_type = MACHINE_OSB;
    }
    else // Must be OS_ATARI_XL
    {
        memcpy(atari_os, ROM_atarios_xl, 0x4000);
        machine_type = MACHINE_XLXE;
    }
    
    // Keep a clean unpatched copy around in VRAM for restoring as needed...
    memcpy(atari_os_pristine, atari_os, 0x4000);
}

// -----------------------------------------------------------
// This routine shows all of the relevant ROM info on
// the lower screen. This includes the name of the ROM
// loaded (XEX or ATR) as well as some relevant info
// on how the emulator is configured - how much memory,
// if BASIC is loaded, etc.
// -----------------------------------------------------------
void dsShowRomInfo(void)
{
    extern char disk_filename[DISK_MAX][256];
    static char line1[25];
    static char ramSizeBuf[8];
    static char machineBuf[20];
    static char line2[200];

    if (!bShowKeyboard)
    {
        if (myConfig.emulatorText)
            dsPrintValue(10,2,0, (file_type == AFILE_CART) ? "CAR:  " : ((file_type == AFILE_ROM) ? "ROM:  " : "XEX:  "));
        else
            dsPrintValue(10,2,0, "      ");

        strncpy(line1, disk_filename[DISK_XEX], 22);
        line1[22] = 0;
        siprintf(line2,"%-22s", line1);
        if (!myConfig.emulatorText) strcpy(line2, "                      ");
        dsPrintValue(10,3,0, line2);

        if (myConfig.emulatorText)
        {
            siprintf(line1, "D1: %s", (disk_readonly[DISK_1] ? "[R]":"[W]"));
            dsPrintValue(10,6,0, line1);
        }
        else
        {
            dsPrintValue(10,6,0, "        ");
        }
        strncpy(line1, disk_filename[DISK_1], 22);
        line1[22] = 0;
        siprintf(line2,"%-22s", line1);
        if (!myConfig.emulatorText) strcpy(line2, "                      ");
        dsPrintValue(10,7,0, line2);
        if (!myConfig.emulatorText) strcpy(line2, "                      ");
        if (strlen(disk_filename[DISK_1]) > 26)
        {
            strncpy(line1, &disk_filename[DISK_1][22], 22);
            line1[22] = 0;
            siprintf(line2,"%-22s", line1);
        }
        else
        {
            siprintf(line2,"%-22s", " ");
        }
        if (!myConfig.emulatorText) strcpy(line2, "                      ");
        dsPrintValue(10,8,0, line2);

        if (myConfig.emulatorText)
        {
            siprintf(line1, "D2: %s", (disk_readonly[DISK_2] ? "[R]":"[W]"));
            dsPrintValue(10,11,0, line1);
        }
        else
        {
            dsPrintValue(10,11,0, "        ");
        }

        strncpy(line1, disk_filename[DISK_2], 22);
        line1[22] = 0;
        siprintf(line2,"%-22s", line1);
        if (!myConfig.emulatorText) strcpy(line2, "                      ");
        dsPrintValue(10,12,0, line2);
        if (strlen(disk_filename[DISK_2]) > 26)
        {
            strncpy(line1, &disk_filename[DISK_2][22], 22);
            line1[22] = 0;
            siprintf(line2,"%-22s", line1);
        }
        else
        {
            siprintf(line2,"%-22s", " ");
        }
        if (!myConfig.emulatorText) strcpy(line2, "                      ");
        dsPrintValue(10,13,0, line2);
    }

    siprintf(ramSizeBuf, "%4dK", ram_size);
    if ((myConfig.os_type == OS_ATARI_OSB) || (myConfig.os_type==OS_ALTIRRA_800))
        siprintf(machineBuf, "%-5s A800", (myConfig.basic_type ? "BASIC": " "));
    else
        siprintf(machineBuf, "%-5s XL/XE", (myConfig.basic_type ? "BASIC": " "));
    siprintf(line2, "%-12s %-4s %-4s", machineBuf, ramSizeBuf, (myConfig.tv_type == TV_NTSC ? "NTSC":"PAL "));
    dsPrintValue(7,0,0, line2);
}

bool isDisk(char *filename)
{
    if (strcasecmp(strrchr(filename, '.'), ".atr") == 0) return TRUE;
    if (strcasecmp(strrchr(filename, '.'), ".ATR") == 0) return TRUE;
    if (strcasecmp(strrchr(filename, '.'), ".atx") == 0) return TRUE;
    if (strcasecmp(strrchr(filename, '.'), ".ATX") == 0) return TRUE;
    return FALSE;
}

// ----------------------------------------------------------------------------------------
// The master routine to read in a XEX or ATR file. We check the A8DS.DAT configuration
// database to see if the hash is found so we can restore user settings for the game.
// ----------------------------------------------------------------------------------------
unsigned int last_crc = 0x55AABEEF;
void dsLoadGame(char *filename, int disk_num, bool bRestart, bool bReadOnly)
{
    if (disk_num == DISK_XEX)   // Force restart on XEX load
    {
        bRestart = true;
    }

    if (disk_num == DISK_1)     strcpy(last_disk_filename[0], filename);
    if (disk_num == DISK_2)     strcpy(last_disk_filename[1], filename);
    if (disk_num == DISK_XEX)   {strcpy(last_disk_filename[0], "EMPTY"); strcpy(last_disk_filename[1], "EMPTY"); last_read_only[0] = 1; last_read_only[1] = 1;} // When loading XEX/CAR, clear out disks

    if (bRestart) // Only save last filename and hash if we are restarting...
    {
        if (strcmp(filename, last_boot_file) != 0)
        {
             strcpy(last_boot_file, filename);
        }
        
        // Get the hash of the file
        if (isDisk(filename))
            last_crc = getFileCrcATR(filename);
        else
            last_crc = getFileCrc(filename);

        // -------------------------------------------------------------------
        // If we are cold starting, go see if we have settings we can read
        // in from a config file or else set some reasonable defaults ...
        // -------------------------------------------------------------------
        ApplyGameSpecificSettings();
        Atari800_Initialise();
    }

    if (strcmp(filename, "EMPTY") != 0) // If we aren't trying to load an EMPTY disk
    {
        // load game if ok
        if (Atari800_OpenFile(filename, bRestart, disk_num, bReadOnly, myConfig.basic_type) != AFILE_ERROR)
        {
          // Initialize the virtual console emulation
          if (bRestart) dsShowScreenEmu();

          bAtariCrash = false;
          dsPrintValue(1,23,0, "                              ");

          dsSetAtariPalette();

          if (bRestart)
          {
              // In case we switched PAL/NTSC
              dsInstallSoundEmuFIFO();

              TIMER2_DATA = TIMER_FREQ(SOUND_FREQ+10);   // Very slightly faster to ensure we always swallow all samples produced by the Pokey
              TIMER2_CR = TIMER_DIV_1 | TIMER_IRQ_REQ | TIMER_ENABLE;
              irqSet(IRQ_TIMER2, VsoundHandler);

              atari_frames = 0;
              TIMER0_CR=0;
              TIMER0_DATA=0;
              TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
          }
      }
    }
      
    dsShowRomInfo();
} // End of dsLoadGame()


// --------------------------------------------------------------
// Read the DS keys to see if we have any relevant presses...
// --------------------------------------------------------------
unsigned int dsReadPad(void)
{
    unsigned int keys_pressed, ret_keys_pressed;

    do
    {
        keys_pressed = keysCurrent();
    } while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);

    do
    {
        keys_pressed = keysCurrent();
    } while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
    ret_keys_pressed = keys_pressed;

    do
    {
        keys_pressed = keysCurrent();
    } while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);

    return ret_keys_pressed;
} // End of dsReadPad()


// ---------------------------------------------------------------
// Called when the user clicks the Exit icon in the upper right
// corner of the main lower display. We ask the user to confirm
// they really want to quit the emulator...
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Called when the user clicks to load or save state
// ---------------------------------------------------------------
bool dsQuery(char *str)
{
    bool bRet=false, bDone=false;
    short unsigned int keys_pressed;
    short unsigned int posdeb=0;
    static char szName[32];

    decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

    strcpy(szName,str);
    dsPrintValue(16-strlen(szName)/2,8,0,szName);
    siprintf(szName,"%s","A TO CONFIRM, B TO GO BACK");
    dsPrintValue(16-strlen(szName)/2,23,0,szName);

    while(!bDone)
    {
        strcpy(szName,"          YES          ");
        dsPrintValue(5,12+0,(posdeb == 0 ? 1 :  0),szName);
        strcpy(szName,"          NO           ");
        dsPrintValue(5,14+1,(posdeb == 2 ? 1 :  0),szName);
        swiWaitForVBlank();

        // Check keypad
        keys_pressed=dsReadPad();
        if (keys_pressed & KEY_UP)
        {
            if (posdeb) posdeb-=2;
        }
        if (keys_pressed & KEY_DOWN)
        {
            if (posdeb<1) posdeb+=2;
        }
        if (keys_pressed & KEY_A)
        {
            bRet = (posdeb ? false : true);
            bDone = true;
        }
        if (keys_pressed & KEY_B)
        {
            bDone = true;
        }
    }

    decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
    dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    dsShowRomInfo();

    return bRet;
}


void dsDisplayLoadOptions(void)
{
    static char tmpBuf[32];

    dsPrintValue(0,0,0,file_load_id);
    siprintf(tmpBuf, "%-4s %s", (myConfig.tv_type == TV_NTSC ? "NTSC":"PAL "), (myConfig.basic_type ? "W BASIC":"       "));
    dsPrintValue(19,0,0,tmpBuf);
    siprintf(tmpBuf, "[%c]  READ-ONLY", (last_read_only[file_load_idx] ? 'X':' '));
    dsPrintValue(14,1,0,tmpBuf);
    if (strcmp(file_load_id,"D2")!=0)      // For D2: we don't allow boot load
    {
        siprintf(tmpBuf, "[%c]  BOOT LOAD", (bLoadAndBoot ? 'Y':' '));
        dsPrintValue(14,2,0,tmpBuf);
    }
}

// ---------------------------------------------------------------------------------
// Show all of the found XEX and ATR files for emulator use on the screen.
// Handles scroling and highlighting. This code has had a number of bugs in
// it and I'm still not sure they are all worked out... but it's _mostly_ okay.
// ---------------------------------------------------------------------------------
void dsDisplayFiles(unsigned int NoDebGame,u32 ucSel)
{
  unsigned int ucBcl,ucGame;
  u8 maxLen;
  static char szName[300];

  // Display all games if possible
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) (bgGetMapPtr(bg1b)),32*24*2);

  dsDisplayLoadOptions();
  countfiles ? siprintf(szName,"%04d/%04d GAMES",(int)(1+ucSel+NoDebGame),countfiles) : siprintf(szName,"%04d/%04d FOLDERS",(int)(1+ucSel+NoDebGame),count8bit);
  dsPrintValue(14,3,0,szName);

  dsPrintValue(31,5,0,(char *) (NoDebGame>0 ? "<" : " "));
  dsPrintValue(31,22,0,(char *) (NoDebGame+14<count8bit ? ">" : " "));
  siprintf(szName,"%s"," A=PICK B=BACK SEL=TV STA=BASIC ");
  dsPrintValue(0,23,0,szName);
  for (ucBcl=0;ucBcl<17; ucBcl++)
  {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < count8bit)
    {
      static char szName2[300];
      maxLen=strlen(a8romlist[ucGame].filename);
      strcpy(szName,a8romlist[ucGame].filename);
      if (maxLen>29) szName[29]='\0';
      if (a8romlist[ucGame].directory)
      {
        char szName3[36];
        siprintf(szName3,"[%s]",szName);
        siprintf(szName2,"%-29s",szName3);
        dsPrintValue(0,5+ucBcl,(ucSel == ucBcl ? 1 :  0),szName2);
      }
      else
      {
        siprintf(szName2,"%-29s",strupr(szName));
        dsPrintValue(1,5+ucBcl,(ucSel == ucBcl ? 1 : 0),szName2);
      }
    }
  }
}


// ---------------------------------------------------------------------------------
// Wait for the user to pick a new ROM for loading into the emulator.
// ---------------------------------------------------------------------------------
unsigned int dsWaitForRom(void)
{
  bool bDone=false, bRet=false;
  u32 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00,romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage, uLenFic=0,ucFlip=0, ucFlop=0;
  static char szName[300];

  decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

  nbRomPerPage = (count8bit>=17 ? 17 : count8bit);
  uNbRSPage = (count8bit>=5 ? 5 : count8bit);
  if (ucFicAct>count8bit)
  {
      firstRomDisplay=0;
      ucFicAct=0;
      romSelected=0;
  }
  if (ucFicAct>count8bit-nbRomPerPage)
  {
    firstRomDisplay=count8bit-nbRomPerPage;
    romSelected=ucFicAct-count8bit+nbRomPerPage;
  }
  else
  {
    firstRomDisplay=ucFicAct;
    romSelected=0;
  }

  force_tv_type = 99;
  force_basic_type = 99;

  dsDisplayFiles(firstRomDisplay,romSelected);
  while (!bDone) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        ucFicAct = (ucFicAct>0 ? ucFicAct-1 : count8bit-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=count8bit-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
      uLenFic=0; ucFlip=0;
    }
    else {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        ucFicAct = (ucFicAct< count8bit-1 ? ucFicAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<count8bit-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
      uLenFic=0; ucFlip=0;
    }
    else {
      ucBas = 0;
    }
    if ((keysCurrent() & KEY_R) || (keysCurrent() & KEY_RIGHT)) {
      if (!ucSBas) {
        ucFicAct = (ucFicAct< count8bit-nbRomPerPage ? ucFicAct+nbRomPerPage : count8bit-nbRomPerPage);
        if (firstRomDisplay<count8bit-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = count8bit-nbRomPerPage; }
        if (ucFicAct == count8bit-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      }
    }
    else {
      ucSBas = 0;
    }
    if ((keysCurrent() & KEY_L) || (keysCurrent() & KEY_LEFT)) {
      if (!ucSHaut) {
        ucFicAct = (ucFicAct> nbRomPerPage ? ucFicAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucFicAct == 0) romSelected = 0;
        if (romSelected > ucFicAct) romSelected = ucFicAct;

        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      }
    }
    else {
      ucSHaut = 0;
    }
    if ( keysCurrent() & KEY_B )
    {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }
    static int last_sel_key = 0;
    if (keysCurrent() & KEY_SELECT)
    {
        if (last_sel_key != KEY_SELECT)
        {
            if (myConfig.tv_type == TV_NTSC)
            {
                myConfig.tv_type = TV_PAL;
                force_tv_type = TV_PAL;
            }
            else
            {
                myConfig.tv_type = TV_NTSC;
                force_tv_type = TV_NTSC;
            }
            dsDisplayLoadOptions();
            last_sel_key = KEY_SELECT;
        }
    } else last_sel_key=0;

    static int last_sta_key = 0;
    if (keysCurrent() & KEY_START)
    {
        if (last_sta_key != KEY_START)
        {
            if (myConfig.basic_type == BASIC_NONE)
            {
                myConfig.basic_type = (bAtariBASIC ? BASIC_ATARIREVC : BASIC_ALTIRRA);
                force_basic_type = myConfig.basic_type;
            }
            else
            {
                myConfig.basic_type = BASIC_NONE;
                force_basic_type = myConfig.basic_type;
            }
            dsDisplayLoadOptions();
            last_sta_key = KEY_START;
        }
    } else last_sta_key=0;


    if (keysCurrent() & KEY_A)
    {
      if (!a8romlist[ucFicAct].directory)
      {
        bRet=true;
        bDone=true;
        DEBUG_DUMP = (keysCurrent() & KEY_X) ? 1:0;
      }
      else
      {
        chdir(a8romlist[ucFicAct].filename);
        a8FindFiles();
        ucFicAct = 0;
        nbRomPerPage = (count8bit>=16 ? 16 : count8bit);
        uNbRSPage = (count8bit>=5 ? 5 : count8bit);
        if (ucFicAct>count8bit-nbRomPerPage) {
          firstRomDisplay=count8bit-nbRomPerPage;
          romSelected=ucFicAct-count8bit+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucFicAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }

    static int last_x_key = 0;
    if (keysCurrent() & KEY_X)
    {
        if (last_x_key != KEY_X)
        {
            last_read_only[file_load_idx] = (last_read_only[file_load_idx] ? false:true);
            dsDisplayLoadOptions();
            last_x_key = KEY_X;
        }
    } else last_x_key = 0;

    static int last_y_key = 0;
    if (keysCurrent() & KEY_Y)
    {
        if (strcmp(file_load_id,"D2")!=0)      // For D2: we don't allow boot load
        {
            if (last_y_key != KEY_Y)
            {
                bLoadAndBoot = (bLoadAndBoot ? false:true);
                dsDisplayLoadOptions();
                last_y_key = KEY_Y;
            }
        }
    } else last_y_key = 0;

    // Scroll the current selection
    if (strlen(a8romlist[ucFicAct].filename) > 29) {
      ucFlip++;
      if (ucFlip >= 10) {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+29)>strlen(a8romlist[ucFicAct].filename)) {
          ucFlop++;
          if (ucFlop >= 10) {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,a8romlist[ucFicAct].filename+uLenFic,29);
        szName[29] = '\0';
        dsPrintValue(1,5+romSelected,1,szName);
      }
    }
    swiWaitForVBlank();
  }

  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

  dsShowRomInfo();

  return bRet;
}


// ----------------------------------------------------------------------------------
// On screen keyboard handling. We have two kinds of keyboard supported - the full
// keyboard with CTRL, ESC and other lesser used keys... and a "simplified" keyboard
// that is a little easier to use for text adventures which contains only the letters
// and some of the numbers. The user can select which keyboard they prefer in the
// options area (gear icon).
// ----------------------------------------------------------------------------------
static u16 shift=0;
static u16 ctrl=0;
void dsShowKeyboard(void)
{
      if (myConfig.keyboard_type == 5) // Star Raiders
      {
          decompress(kbd_starraidTiles, bgGetGfxPtr(bg0b), LZ77Vram);
          decompress(kbd_starraidMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
          dmaCopy((void *) kbd_starraidPal,(u16*) BG_PALETTE_SUB,256*2);
          unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
          dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
      }
      else if (myConfig.keyboard_type == 4) // Alphanumeric
      {
          decompress(kbd_alphaTiles, bgGetGfxPtr(bg0b), LZ77Vram);
          decompress(kbd_alphaMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
          dmaCopy((void *) kbd_alphaPal,(u16*) BG_PALETTE_SUB,256*2);
          unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
          dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
      }
      else if (myConfig.keyboard_type == 3) // XE style
      {
          decompress(kbd_XETiles, bgGetGfxPtr(bg0b), LZ77Vram);
          decompress(kbd_XEMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
          dmaCopy((void *) kbd_XEPal,(u16*) BG_PALETTE_SUB,256*2);
          unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
          dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
      }
      else if (myConfig.keyboard_type == 2) // 400 style
      {
          decompress(kbd_400Tiles, bgGetGfxPtr(bg0b), LZ77Vram);
          decompress(kbd_400Map, (void*) bgGetMapPtr(bg0b), LZ77Vram);
          dmaCopy((void *) kbd_400Pal,(u16*) BG_PALETTE_SUB,256*2);
          unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
          dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
      }
      else if (myConfig.keyboard_type == 1) // XL2 style
      {
          decompress(kbd_XL2Tiles, bgGetGfxPtr(bg0b), LZ77Vram);
          decompress(kbd_XL2Map, (void*) bgGetMapPtr(bg0b), LZ77Vram);
          dmaCopy((void *) kbd_XL2Pal,(u16*) BG_PALETTE_SUB,256*2);
          unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
          dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
      }
      else // XL Style
      {
          decompress(kbd_XLTiles, bgGetGfxPtr(bg0b), LZ77Vram);
          decompress(kbd_XLMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
          dmaCopy((void *) kbd_XLPal,(u16*) BG_PALETTE_SUB,256*2);
          unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
          dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
      }
      swiWaitForVBlank();
      dsShowRomInfo();
}


// ----------------------------------------------------------------------------------
// After showing the help screen or one of the soft keyboard screens, we can call
// this routine to restore the normal lower screen which shows the emulator and
// various bits of information about what ROM is loaded and running...
// ----------------------------------------------------------------------------------
void dsRestoreBottomScreen(void)
{
    decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    swiWaitForVBlank();
    dsShowRomInfo();
}

// ------------------------------------------------------------------------------------
// This is the handler when the emulator first starts without a ROM loaded... the user
// is forced to pick a game to start running - or else they can quit the emulator.
// ------------------------------------------------------------------------------------
unsigned int dsWaitOnMenu(unsigned int actState)
{
  unsigned int uState=A8_PLAYINIT;
  unsigned int keys_pressed;
  u8 bDone=false, romSel=false;
  short int iTx,iTy;

  while (!bDone)
  {
    // wait for touch screen... user must select a game to load
    keys_pressed = keysCurrent();
    if (keys_pressed & KEY_TOUCH)
    {
        touchPosition touch;
        touchRead(&touch);
        iTx = touch.px;
        iTy = touch.py;

        if ((iTx>204) && (iTx<240) && (iTy>150) && (iTy<185))  // Gear Icon = Global Settings
        {
            dsChooseOptions(FALSE);
        }
        else if ((iTx>230) && (iTx<256) && (iTy>8) && (iTy<30))  // POWER / QUIT
        {
            soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
            bDone=dsQuery("QUIT A8DS?");
            if (bDone) uState=A8_QUITSTDS;
        }
        else if ((iTx>5) && (iTx<80) && (iTy>12) && (iTy<=75))  // XEX/CAR and D1 Disk Drive
        {
            // Find files in current directory and show it
            strcpy(file_load_id, "XEX/CAR/D1");
            file_load_idx = 0;
            chdir(last_working_dir[0]);
            ucFicAct = last_file_index[0];
            a8FindFiles();
            romSel=dsWaitForRom();
            if (romSel) 
            { 
                last_file_index[0] = ucFicAct;
                getcwd(last_working_dir[0], 299);
                if (bLoadAndBoot) {uState=A8_PLAYINIT; bDone=true;} else uState=actState;
                dsLoadGame(a8romlist[ucFicAct].filename, DISK_1, bLoadAndBoot, last_read_only[0]); 
            }
            else { uState=actState; }
        }
        else if ((iTx>5) && (iTx<80) && (iTy>75) && (iTy<114)) // D2 Disk Drive
        {
            // Find files in current directory and show it
            strcpy(file_load_id, "D2");
            file_load_idx = 1;
            chdir(last_working_dir[1]);
            ucFicAct = last_file_index[1];
            a8FindFiles();
            romSel=dsWaitForRom();
            if (romSel) 
            { 
                last_file_index[1] = ucFicAct;
                getcwd(last_working_dir[1], 299);
                uState=actState;
                dsLoadGame(a8romlist[ucFicAct].filename, DISK_2, false, last_read_only[1]); 
            }
            else { uState=actState; }
        }
        else if ((iTx>192) && (iTx<250) && (iTy>120) && (iTy<143))  // RESET (load the game... user might have been mounting disks)
        {
            if (romSel)
            {
                soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                dsLoadGame(last_disk_filename[0], DISK_1, true,  last_read_only[0]);  // Mount D1 and force restart
                dsLoadGame(last_disk_filename[1], DISK_2, false, last_read_only[1]);  // Mount D2 if available (must be done after D1)
                uState=A8_PLAYINIT; 
                bDone=true;
            }
        }
    }
    swiWaitForVBlank();
  }

  return uState;
}

// ---------------------------------------------------------------------------------------
// Utility function that sees a ton of use - this puts out a generic string onto
// the lower display at the desired X/Y position. The display is broken down to
// 32 horizontal characters and 24 vertical ones (so don't think of X/Y as pixels).
// Since we use two independent backgrounds on the lower display, this text is nicely
// overlaid on top of the background graphic without disturbing those pixels.
// ---------------------------------------------------------------------------------------
void dsPrintValue(int x, int y, unsigned int isSelect, char *pchStr)
{
  u16 *pusEcran,*pusMap;
  u16 usCharac;
  char *pTrTxt=pchStr;
  char ch;

  pusEcran=(u16*) (bgGetMapPtr(bg1b))+x+(y<<5);
  pusMap=(u16*) (bgGetMapPtr(bg0b)+(2*isSelect+24)*32);

  while((*pTrTxt)!='\0' )
  {
    ch = *pTrTxt;
    if (ch >= 'a' && ch <= 'z') ch -= 32; // Faster than strcpy/strtoupper
    usCharac=0x0000;
    if ((ch) == '|')
      usCharac=*(pusMap);
    else if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');
    else
      usCharac=*(pusMap+32+(ch)-'@');
    *pusEcran++=usCharac;
    pTrTxt++;
  }
}


// -------------------------------------------
// Normal Keyboard handler...
// -------------------------------------------
static u8 keyboard_debounce=0;
int dsHandleKeyboard(int Tx, int Ty)
{
    int keyPress = AKEY_NONE;

    if (Ty <= 8) return AKEY_NONE;

    if (Ty > 14 && Ty < 49)       // Top Row 0-9
    {
        if (Tx < 3) keyPress = AKEY_NONE;
        else if (Tx <  23) keyPress = (shift ? AKEY_EXCLAMATION : AKEY_1);
        else if (Tx <  41) keyPress = (shift ? AKEY_DBLQUOTE    : AKEY_2);
        else if (Tx <  60) keyPress = (shift ? AKEY_HASH        : AKEY_3);
        else if (Tx <  77) keyPress = (shift ? AKEY_DOLLAR      : AKEY_4);
        else if (Tx <  98) keyPress = (shift ? AKEY_PERCENT     : AKEY_5);
        else if (Tx < 117) keyPress = (shift ? AKEY_AMPERSAND   : AKEY_6);
        else if (Tx < 136) keyPress = (shift ? AKEY_QUOTE       : AKEY_7);
        else if (Tx < 155) keyPress = (shift ? AKEY_AT          : AKEY_8);
        else if (Tx < 174) keyPress = (shift ? AKEY_PARENLEFT   : AKEY_9);
        else if (Tx < 193) keyPress = (shift ? AKEY_PARENRIGHT  : AKEY_0);
        else if (Tx < 212) keyPress = (shift ? AKEY_CLEAR       : AKEY_LESS);
        else if (Tx < 231) keyPress = (shift ? AKEY_INSERT_CHAR : AKEY_GREATER);
        else if (Tx < 250) keyPress = (shift ? AKEY_DELETE_CHAR : AKEY_BACKSPACE);
    }
    else if (Ty < 85)  // Row QWERTY
    {
        if (Tx < 14) keyPress = AKEY_NONE;
        else if (Tx <  33) keyPress = (shift ? AKEY_Q : AKEY_q);
        else if (Tx <  52) keyPress = (shift ? AKEY_W : AKEY_w);
        else if (Tx <  71) keyPress = (shift ? AKEY_E : AKEY_e);
        else if (Tx <  90) keyPress = (shift ? AKEY_R : AKEY_r);
        else if (Tx < 109) keyPress = (shift ? AKEY_T : AKEY_t);
        else if (Tx < 128) keyPress = (shift ? AKEY_Y : AKEY_y);
        else if (Tx < 147) keyPress = (shift ? AKEY_U : AKEY_u);
        else if (Tx < 166) keyPress = (shift ? AKEY_I : AKEY_i);
        else if (Tx < 185) keyPress = (shift ? AKEY_O : AKEY_o);
        else if (Tx < 204) keyPress = (shift ? AKEY_P : AKEY_p);
        else if (Tx < 223) keyPress = (shift ? AKEY_UNDERSCORE : AKEY_MINUS);
        else if (Tx < 242) keyPress = (shift ? AKEY_BAR : AKEY_EQUAL);
    }
    else if (Ty < 121)  // Home Row ASDF-JKL;
    {
        if (Tx < 19)       keyPress = AKEY_TAB;
        else if (Tx <  38) keyPress = (shift ? AKEY_A : AKEY_a);
        else if (Tx <  57) keyPress = (shift ? AKEY_S : AKEY_s);
        else if (Tx <  76) keyPress = (shift ? AKEY_D : AKEY_d);
        else if (Tx <  95) keyPress = (shift ? AKEY_F : AKEY_f);
        else if (Tx < 114) keyPress = (shift ? AKEY_G : AKEY_g);
        else if (Tx < 133) keyPress = (shift ? AKEY_H : AKEY_h);
        else if (Tx < 152) keyPress = (shift ? AKEY_J : AKEY_j);
        else if (Tx < 171) keyPress = (shift ? AKEY_K : AKEY_k);
        else if (Tx < 190) keyPress = (shift ? AKEY_L : AKEY_l);
        else if (Tx < 209) keyPress = (shift ? AKEY_COLON : AKEY_SEMICOLON);
        else if (Tx < 228) keyPress = (shift ? AKEY_BACKSLASH : AKEY_PLUS);
        else if (Tx < 247) keyPress = (shift ? AKEY_CARET : AKEY_ASTERISK);
    }
    else if (Ty < 157)  // ZXCV Row
    {
        if (Tx < 30)       keyPress = AKEY_CTRL;
        else if (Tx <  49) keyPress = (shift ? AKEY_Z : AKEY_z);
        else if (Tx <  68) keyPress = (shift ? AKEY_X : AKEY_x);
        else if (Tx <  87) keyPress = (shift ? AKEY_C : AKEY_c);
        else if (Tx < 106) keyPress = (shift ? AKEY_V : AKEY_v);
        else if (Tx < 125) keyPress = (shift ? AKEY_B : AKEY_b);
        else if (Tx < 144) keyPress = (shift ? AKEY_N : AKEY_n);
        else if (Tx < 163) keyPress = (shift ? AKEY_M : AKEY_m);
        else if (Tx < 182) keyPress = (shift ? AKEY_BRACKETLEFT : AKEY_COMMA);
        else if (Tx < 201) keyPress = (shift ? AKEY_BRACKETRIGHT : AKEY_FULLSTOP);
        else if (Tx < 220) keyPress = (shift ? AKEY_QUESTION : AKEY_SLASH);
        else if (Tx < 255) keyPress = AKEY_RETURN;
    }
    else if (Ty < 192)  // Spacebar Row
    {
        if (Tx <  27) keyPress = AKEY_EXIT;
        else if (Tx <  46) keyPress = AKEY_ESCAPE;
        else if (Tx <  66) keyPress = AKEY_SHFT;
        else if (Tx < 200) keyPress = AKEY_SPACE;
        else if (Tx < 218) keyPress = AKEY_SHFT;
        else if (Tx < 237) keyPress = AKEY_CAPSTOGGLE;
        else if (Tx < 255) keyPress = AKEY_BREAK;
    }

    if (keyPress == AKEY_SHFT)
    {
        if ( !keyboard_debounce )   // To prevent from toggling so quickly...
        {
            keyboard_debounce=15;
            if (shift == 0)
            {
                shift = 1;
                dsPrintValue(0,0,0, "SHF");
            }
            else
            {
                shift = 0;
                dsPrintValue(0,0,0, "   ");
            }
        }
        keyPress = AKEY_NONE;
    }
    else if (keyPress == AKEY_CTRL)
    {
        if ( !keyboard_debounce )   // To prevent from toggling so quickly...
        {
            keyboard_debounce=15;
            ctrl ^= 1;
            dsPrintValue(0,0,0, (ctrl ? "CTL":"   "));
        }
        keyPress = AKEY_NONE;
    }
    else if (ctrl)
    {
        keyPress |= AKEY_CTRL;
        ctrl = 0;
        dsPrintValue(0,0,0, "   ");
    }
    else if (keyPress != AKEY_NONE)
    {
        ctrl=0;
        shift=0;
        dsPrintValue(0,0,0, "   ");
    }

    return keyPress;
}



// -----------------------------------------------------------------------------------------------
// A small buffer to allow keys to be buffered for the Alpha-numeric keyboard and the macro keys
// -----------------------------------------------------------------------------------------------
short int push_key_codes[12];
u8 push_key_idx = 0;

void PushKey(u8 key)
{
    if (push_key_idx < 12) push_key_codes[push_key_idx++] = key;
}

static short int last_key_code = -1;

int PullKey(void)
{
    int key = AKEY_NONE;
    if (push_key_idx)
    {
        static u8 dampen = 0;
        key = push_key_codes[push_key_idx-1];
        if (dampen == 0) soundPlaySample(keyclick_wav, SoundFormat_16Bit, keyclick_wav_size, 44100, 127, 64, false, 0);
        if (++dampen > 5)
        {
            key = AKEY_NONE;
            last_key_code = AKEY_NONE;
            if (++dampen > 9)
            {
                push_key_idx--;
                dampen = 0;
            }
        }
    }

    return key;
}

int dsHandleKeyboardAlpha(int Tx, int Ty)
{
    int keyPress = AKEY_NONE;

    if (Ty < 12) return AKEY_NONE;
    if (Ty < 41)       // MACRO ROW
    {
             if (Tx <  52) {PushKey(AKEY_SPACE);PushKey(AKEY_E);PushKey(AKEY_K);PushKey(AKEY_A);PushKey(AKEY_T);} // TAKE
        else if (Tx < 102) {PushKey(AKEY_SPACE);PushKey(AKEY_P);PushKey(AKEY_O);PushKey(AKEY_R);PushKey(AKEY_D);} // DROP
        else if (Tx < 153) {PushKey(AKEY_SPACE);PushKey(AKEY_K);PushKey(AKEY_O);PushKey(AKEY_O);PushKey(AKEY_L);} // LOOK
        else if (Tx < 203) {PushKey(AKEY_SPACE);PushKey(AKEY_E);PushKey(AKEY_N);PushKey(AKEY_I);PushKey(AKEY_M);PushKey(AKEY_A);PushKey(AKEY_X);PushKey(AKEY_E);} // EXAMINE
        else if (Tx < 256) {PushKey(AKEY_SPACE);PushKey(AKEY_N);PushKey(AKEY_E);PushKey(AKEY_P);PushKey(AKEY_O);} // OPEN
    }
    else if (Ty < 81)   // Top Row QWERTY
    {
             if (Tx <  28) keyPress = (shift ? AKEY_Q : AKEY_q);
        else if (Tx <  53) keyPress = (shift ? AKEY_W : AKEY_w);
        else if (Tx <  78) keyPress = (shift ? AKEY_E : AKEY_e);
        else if (Tx < 103) keyPress = (shift ? AKEY_R : AKEY_r);
        else if (Tx < 128) keyPress = (shift ? AKEY_T : AKEY_t);
        else if (Tx < 153) keyPress = (shift ? AKEY_Y : AKEY_y);
        else if (Tx < 178) keyPress = (shift ? AKEY_U : AKEY_u);
        else if (Tx < 203) keyPress = (shift ? AKEY_I : AKEY_i);
        else if (Tx < 228) keyPress = (shift ? AKEY_O : AKEY_o);
        else if (Tx < 256) keyPress = (shift ? AKEY_P : AKEY_p);
    }
    else if (Ty < 120)  // Home Row ASDF
    {
             if (Tx <  28) keyPress = (shift ? AKEY_A : AKEY_a);
        else if (Tx <  53) keyPress = (shift ? AKEY_S : AKEY_s);
        else if (Tx <  78) keyPress = (shift ? AKEY_D : AKEY_d);
        else if (Tx < 103) keyPress = (shift ? AKEY_F : AKEY_f);
        else if (Tx < 128) keyPress = (shift ? AKEY_G : AKEY_g);
        else if (Tx < 153) keyPress = (shift ? AKEY_H : AKEY_h);
        else if (Tx < 178) keyPress = (shift ? AKEY_J : AKEY_j);
        else if (Tx < 203) keyPress = (shift ? AKEY_K : AKEY_k);
        else if (Tx < 228) keyPress = (shift ? AKEY_L : AKEY_l);
        else if (Tx < 256) keyPress = AKEY_BACKSPACE;
    }
    else if (Ty < 160)  // Bottom Row ZXCV
    {
             if (Tx <  28) keyPress = (shift ? AKEY_Z : AKEY_z);
        else if (Tx <  53) keyPress = (shift ? AKEY_X : AKEY_x);
        else if (Tx <  78) keyPress = (shift ? AKEY_C : AKEY_c);
        else if (Tx < 103) keyPress = (shift ? AKEY_V : AKEY_v);
        else if (Tx < 128) keyPress = (shift ? AKEY_B : AKEY_b);
        else if (Tx < 153) keyPress = (shift ? AKEY_N : AKEY_n);
        else if (Tx < 178) keyPress = (shift ? AKEY_M : AKEY_m);
        else if (Tx < 203) keyPress = AKEY_FULLSTOP;
        else if (Tx < 228) keyPress = AKEY_RETURN;
        else if (Tx < 256) keyPress = AKEY_RETURN;
    }
    else if (Ty < 192)  // Space Bar - Menu Row
    {
             if (Tx <  21) keyPress = AKEY_1;
        else if (Tx <  40) keyPress = AKEY_2;
        else if (Tx <  59) keyPress = AKEY_3;
        else if (Tx <  78) keyPress = AKEY_4;
        else if (Tx < 197) keyPress = AKEY_SPACE;
        else if (Tx < 222) keyPress = AKEY_CAPSTOGGLE;
        else keyPress = AKEY_EXIT;
    }

    if (keyPress == AKEY_SHFT)
    {
        if ( !keyboard_debounce )   // To prevent from toggling so quickly...
        {
            keyboard_debounce=15;
            if (shift == 0)
            {
                shift = 1;
                dsPrintValue(0,0,0, "SHF");
            }
            else
            {
                shift = 0;
                dsPrintValue(0,0,0, "   ");
            }
        }
        keyPress = AKEY_NONE;
    }
    else if (keyPress == AKEY_CTRL)
    {
        if ( !keyboard_debounce )   // To prevent from toggling so quickly...
        {
            keyboard_debounce=15;
            ctrl ^= 1;
            dsPrintValue(0,0,0, (ctrl ? "CTL":"   "));
        }
        keyPress = AKEY_NONE;
    }
    else if (ctrl)
    {
        keyPress |= AKEY_CTRL;
        ctrl = 0;
        dsPrintValue(0,0,0, "   ");
    }
    else if (keyPress != AKEY_NONE)
    {
        ctrl=0;
        shift=0;
        dsPrintValue(0,0,0, "   ");
    }

    return keyPress;
}

int dsHandleStarRaidersKeyboard(int Tx, int Ty)
{
    int keyPress = AKEY_NONE;

    if (Ty <= 12) return AKEY_NONE;

    if (Ty < 59)       // Top Row
    {
        if      (Tx <  32) keyPress = (AKEY_f);
        else if (Tx <  64) keyPress = (AKEY_f);
        else if (Tx <  96) keyPress = (AKEY_l);
        else if (Tx < 128) keyPress = (AKEY_l);
        else if (Tx < 160) keyPress = (AKEY_t);
        else if (Tx < 192) keyPress = (AKEY_t);
        else if (Tx < 224) keyPress = (AKEY_c);
        else if (Tx < 255) keyPress = (AKEY_c);
    }
    else if (Ty < 105)  // Second Row
    {
        if      (Tx <  32) keyPress = (AKEY_a);
        else if (Tx <  64) keyPress = (AKEY_a);
        else if (Tx <  96) keyPress = (AKEY_g);
        else if (Tx < 128) keyPress = (AKEY_g);
        else if (Tx < 160) keyPress = (AKEY_s);
        else if (Tx < 192) keyPress = (AKEY_s);
        else if (Tx < 224) keyPress = (AKEY_m);
        else if (Tx < 255) keyPress = (AKEY_m);
    }
    else if (Ty < 146)  // Third Row
    {
        if      (Tx <  32) keyPress = (AKEY_1);
        else if (Tx <  64) keyPress = (AKEY_2);
        else if (Tx <  96) keyPress = (AKEY_3);
        else if (Tx < 128) keyPress = (AKEY_4);
        else if (Tx < 160) keyPress = (AKEY_5);
        else if (Tx < 192) keyPress = (AKEY_6);
        else if (Tx < 224) keyPress = (AKEY_7);
        else if (Tx < 255) keyPress = (AKEY_8);
    }
    else if (Ty < 192)  // Fourth Row
    {
        if      (Tx <  40) keyPress = AKEY_EXIT;
        else if (Tx <  95) keyPress = (AKEY_0);
        else if (Tx < 160) keyPress = (AKEY_h);
        else if (Tx < 215) keyPress = (AKEY_9);
        else if (Tx < 255) keyPress = (AKEY_p);
    }

    return keyPress;
}

// -----------------------------------------------------------------------
// Install the sound emulation - sets up a FIFO with the ARM7 processor.
// -----------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void)
{
    static int last_sound_fifo = -1;
    if (last_sound_fifo == -1)
    {
        bMute = 0;
        last_sound_fifo = 1;
        // We are going to use the 16-bit sound engine so we need to scale up our 8-bit values...
        for (int i=0; i<256; i++)
        {
            sampleExtender[i] = (i << 8);
        }

        memset(sound_buffer, 0x00, sizeof(sound_buffer));
        if (isDSiMode())
        {
            aptr = (u16*) ((u32)&sound_buffer[0] + 0xA000000);
            bptr = (u16*) ((u32)&sound_buffer[2] + 0xA000000);
        }
        else
        {
            aptr = (u16*) ((u32)&sound_buffer[0] + 0x00400000);
            bptr = (u16*) ((u32)&sound_buffer[2] + 0x00400000);
        }
        swiWaitForVBlank();swiWaitForVBlank();    // Wait 2 vertical blanks... that's enough for the ARM7 to stop...

        FifoMessage msg;
        msg.SoundPlay.data = &sound_buffer;
        msg.SoundPlay.freq = SOUND_FREQ*2;
        msg.SoundPlay.volume = 127;
        msg.SoundPlay.pan = 64;
        msg.SoundPlay.loop = 1;
        msg.SoundPlay.format = ((1)<<4) | SoundFormat_16Bit;
        msg.SoundPlay.loopPoint = 0;
        msg.SoundPlay.dataSize = 4 >> 2;
        msg.type = EMUARM7_PLAY_SND;
        fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);

        swiWaitForVBlank();swiWaitForVBlank();    // Wait 2 vertical blanks... that's enough for the ARM7 to start chugging...
    }
}

// Called roughly every 1/60th of a second...
void dsHandleDiskSounds(void)
{
    static int soundID=0;
    static u8 play_sio_sound_dampen = 0;

    if (play_sio_sound)
    {
        if (--play_sio_sound)
        {
            if (play_sio_sound_dampen == 0)
            {
                if (myConfig.disk_sound) // Only if sound is enabled...
                {
                    soundID = soundPlaySample(sio_wav, SoundFormat_16Bit, sio_wav_size, 11025, 75, 64, true, 0);
                }
                play_sio_sound_dampen = 1;
            }
        }
        else
        {
            soundKill(soundID);
            dsPrintValue(3,0,0, "   ");
            play_sio_sound_dampen = 0;
        }
    }
}

void dsZoomScreen(void)
{
    u32 savedTimer0 = TIMER0_DATA;
    bMute = 1;
    bScreenZoom = 1;    // vblankIntr() will zoom screen now
    while (keysCurrent() & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_Y | KEY_X | KEY_L | KEY_R | KEY_START | KEY_SELECT)) {WAITVBL;}   // Wait for release
    dsPrintValue(0,0,0, "ZOOM MODE");
    while (true)
    {
        u16 keys = keysCurrent();

        if (keys & (KEY_A | KEY_B | KEY_Y | KEY_X | KEY_L | KEY_R | KEY_START | KEY_SELECT)) break;
        if (keys & KEY_RIGHT)  {if (bZoomX < 64)  bZoomX++; swiWaitForVBlank();}
        if (keys & KEY_LEFT)   {if (bZoomX > 0)   bZoomX--; swiWaitForVBlank();}
        if (keys & KEY_DOWN)   {if (bZoomY < 32)  bZoomY++; swiWaitForVBlank();}
        if (keys & KEY_UP)     {if (bZoomY > -16) bZoomY--; swiWaitForVBlank();}
    }

    bScreenZoom = 0; // Back to normal screen handling
    while (keysCurrent() & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_Y | KEY_X | KEY_L | KEY_R | KEY_START | KEY_SELECT)) {WAITVBL;}   // Wait for release
    dsPrintValue(0,0,0, "         ");
    bMute = 0;
    TIMER0_CR=0;
    TIMER0_DATA=savedTimer0;
    TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
}

// -------------------------------------------------------------------------------
// And finally the main loop! This sits in a forever loop and calls into the
// emulator routines every frame to process 1 frames worth of emulation. If
// we are running in NTSC TV mode, we run the loop 60 times per second and if
// we are in PAL TV mode, we run the loop 50 times per second.  On every frame
// we check for keys to be pressed on the DS and the touch screen and process
// those as appopriate.
// -------------------------------------------------------------------------------
char fpsbuf[32];
u16 nds_keys[8] = {KEY_A, KEY_B, KEY_X, KEY_Y, KEY_L, KEY_R, KEY_START, KEY_SELECT};

void dsMainLoop(void)
{
  static unsigned short int config_snap_counter=0;
  unsigned short int keys_pressed,keys_touch=0, romSel=0;
  short int iTx,iTy;

  // Timers are fed with 33.513982 MHz clock on the NDS.
  // With DIV_1024 the clock is 32,728.5 ticks per sec...
  TIMER0_DATA=0;
  TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;

  myConfig.xOffset = 32;
  myConfig.yOffset = 24;
  myConfig.xScale = 256;
  myConfig.yScale = 256;
  
  strcpy(last_disk_filename[0], "EMPTY");
  strcpy(last_disk_filename[1], "EMPTY");

  while(emu_state != A8_QUITSTDS)
  {
    switch (emu_state)
    {
      case A8_MENUINIT:
        dsShowScreenMain();
        emu_state = A8_MENUSHOW;
        break;

      case A8_MENUSHOW:
        Atari800_Initialise();
        emu_state = dsWaitOnMenu(A8_MENUSHOW);
        dsLoadGame(last_disk_filename[1], DISK_2, false, last_read_only[1]);  // Mount D2 if available
        break;

      case A8_PLAYINIT:
        dsShowScreenEmu();
        irqEnable(IRQ_TIMER2);
        bMute = 0;
        emu_state = A8_PLAYGAME;

        if (myConfig.keyboard_type == 5) // Star Raiders
        {
            bShowKeyboard = true;
            dsShowKeyboard();
        }
        break;

      case A8_PLAYGAME:
        // 32,728.5 ticks = 1 second
        // 1 frame = 1/50 or 1/60 (0.02 or 0.016)
        // 655 -> 50 fps and 546 -> 60 fps
        if (myConfig.fps_setting < 2)
        {
            while(TIMER0_DATA < ((myConfig.tv_type == TV_NTSC ? 546:656)*atari_frames))
                ;
        }

        // ------------------------------------------------------------------------
        // Execute one Atari frame... this is where all of the emulation stuff
        // comes into play. Many thousands of CPU calls and GTIA/Antic accesses
        // are all kicked off via this call to handle just a single Atari 800
        // frame. All of the NTSC and PAL scanlines are done here - and this is
        // where the Nitnendo DS is spending most of its CPU time.
        // ------------------------------------------------------------------------
        Atari800_Frame();

        // ----------------------------------------------------
        // If we have processed 60/50 frames we start anew...
        // ----------------------------------------------------
        if (++atari_frames >= (myConfig.tv_type == TV_NTSC ? 60:50))
        {
            TIMER0_CR=0;
            TIMER0_DATA=0;
            TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
            atari_frames=0;
        }

        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= 32728)   // 1000MS (1 sec)
        {
            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;

            if (gTotalAtariFrames == (myConfig.tv_type == TV_NTSC ? 61:51)) gTotalAtariFrames = (myConfig.tv_type == TV_NTSC ? 60:50);
            if (myConfig.fps_setting)
            {
                fpsbuf[0] = '0' + gTotalAtariFrames / 100;
                fpsbuf[1] = '0' + (gTotalAtariFrames % 100) / 10;
                fpsbuf[2] = '0' + (gTotalAtariFrames % 100) % 10;
                fpsbuf[3] = 0;
                dsPrintValue(0,0,0, fpsbuf); // Show FPS
                if (myConfig.fps_setting==2) dsPrintValue(30,0,0,"FS");
            }
            gTotalAtariFrames = 0;
            DumpDebugData();
            if(bAtariCrash) dsPrintValue(1,23,0, "GAME CRASH - PICK ANOTHER GAME");
        }

        dsHandleDiskSounds();

        // --------------------------------------------
        // Read DS/DSi keys and process them below...
        // --------------------------------------------
        keys_pressed=keysCurrent();
        key_consol = CONSOL_NONE;
        key_shift = 0;
        key_code = AKEY_NONE;
        stick0 = STICK_CENTRE;
        stick1 = STICK_CENTRE;

        if (keyboard_debounce > 0) keyboard_debounce--;

        // ------------------------------------------------------
        // Check if the touch screen pressed and handle it...
        // ------------------------------------------------------
        if (push_key_idx) key_code = PullKey();
        else
        if (keys_pressed & KEY_TOUCH)
        {
            touchPosition touch;
            touchRead(&touch);
            iTx = touch.px;
            iTy = touch.py;

            // ---------------------------------------------------------------------------------------
            // START, SELECT and OPTION respond immediately - that is, we keep the buttons pressed
            // as long as the user is continuing to hold down the button on the touch screen...
            // ---------------------------------------------------------------------------------------
            if (!bShowKeyboard)
            {
                if ((iTx>15) && (iTx<66) && (iTy>120) && (iTy<143))  // START
                {
                    if (!keys_touch)
                    {
                        soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    }
                    keys_touch=1;
                    key_consol &= ~CONSOL_START;
                }
                else if ((iTx>73) && (iTx<127) && (iTy>120) && (iTy<143))  // SELECT
                {
                    if (!keys_touch)
                    {
                        soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    }
                    keys_touch=1;
                    key_consol &= ~CONSOL_SELECT;
                }
                else if ((iTx>133) && (iTx<186) && (iTy>120) && (iTy<143))  // OPTION
                {
                    if (!keys_touch)
                    {
                        soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    }
                    keys_touch=1;
                    key_consol &= ~CONSOL_OPTION;
                }
            }

            if (!keys_touch)
            {
                if (bShowKeyboard)
                {
                      if (bShowKeyboard)
                      {
                         if (myConfig.keyboard_type == 5)
                             key_code = dsHandleStarRaidersKeyboard(iTx, iTy);
                         else if (myConfig.keyboard_type == 4)
                             key_code = dsHandleKeyboardAlpha(iTx, iTy);
                         else
                             key_code = dsHandleKeyboard(iTx, iTy);

                         if (key_code == AKEY_EXIT)
                         {
                              bShowKeyboard = false;
                              keys_touch = 1;
                              dsRestoreBottomScreen();
                              key_code = AKEY_NONE;
                         }
                         else if (key_code != AKEY_NONE)
                         {
                              if ((last_key_code == key_code) || (last_key_code == AKEY_NONE))
                              {
                                  if (last_key_code == AKEY_NONE)
                                  {
                                      if (!myConfig.key_click_disable) soundPlaySample(keyclick_wav, SoundFormat_16Bit, keyclick_wav_size, 44100, 127, 64, false, 0);
                                      last_key_code = key_code;
                                  }
                              }
                              else key_code = AKEY_NONE;
                         }
                         else // Must be AKEY_NONE
                         {
                             last_key_code = AKEY_NONE;
                         }
                      }
                      else
                      {
                          keys_touch = 1;
                          dsRestoreBottomScreen();
                      }
                }
                else
                {
                    if ((iTx>192) && (iTx<250) && (iTy>120) && (iTy<143))  // RESET (reloads the game)
                    {
                        if (!keys_touch)
                        {
                            soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                        }
                        keys_touch = 1;
                        dsLoadGame(last_boot_file, (isDisk(last_boot_file)) ? DISK_1 : DISK_XEX, true, last_read_only[0]);   // Reload the last bootable game
                        dsLoadGame(last_disk_filename[1], DISK_2,   false, last_read_only[1]);  // Mount D2 if available (must be done after XEX/CAR/D1)
                        bMute = 0;
                        swiWaitForVBlank();
                    }
                    else if ((iTx>35) && (iTx<55) && (iTy>145) && (iTy<185))  // HighScore
                    {
                        if (!keys_touch)
                        {
                            soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                        }
                        highscore_display();
                        swiWaitForVBlank();
                        dsRestoreBottomScreen();
                        keys_touch = 1;
                    }
                    else if ((iTx>88) && (iTx<175) && (iTy>150) && (iTy<180))  // Keyboard
                    {
                        if (!keys_touch)
                        {
                            soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                        }
                        bShowKeyboard = true;
                        dsShowKeyboard();
                        keys_touch = 1;
                    }
                    else if ((iTx>230) && (iTx<256) && (iTy>8) && (iTy<30))  // POWER / QUIT
                    {
                      u16 savedTimer0 = TIMER0_DATA;
                      bMute = 1;
                      swiWaitForVBlank();
                      soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                      if (dsQuery("QUIT A8DS?")) emu_state=A8_QUITSTDS;
                      else
                      {
                           TIMER0_CR=0;
                           TIMER0_DATA=savedTimer0;
                           TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
                           bMute = 0;
                      }
                    }
                    else if ((iTx>230) && (iTx<256) && (iTy>46) && (iTy<75))  // SAVE GAME
                    {
                        u16 savedTimer0 = TIMER0_DATA;
                        bMute = 1;
                        if (dsQuery("SAVE GAME STATE?"))
                        {
                          SaveGame();
                        }
                        swiWaitForVBlank();
                        TIMER0_CR=0;
                        TIMER0_DATA=savedTimer0;
                        TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
                        bMute = 0;
                    }
                    else if ((iTx>230) && (iTx<256) && (iTy>85) && (iTy<111))  // LOAD GAME
                    {
                        u16 savedTimer0 = TIMER0_DATA;
                        bMute = 1;
                        if (dsQuery("LOAD GAME STATE?"))
                        {
                          LoadGame();
                        }
                        swiWaitForVBlank();
                        TIMER0_CR=0;
                        TIMER0_DATA=savedTimer0;
                        TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
                        bMute = 0;
                    }
                    else if ((iTx>204) && (iTx<235) && (iTy>150) && (iTy<180))  // Gear Icon = Settings
                    {
                      u16 savedTimer0 = TIMER0_DATA;
                      bMute = 1;
                      swiWaitForVBlank();
                      keys_touch=1;
                      dsChooseOptions(TRUE);
                      swiWaitForVBlank();
                      TIMER0_CR=0;
                      TIMER0_DATA=savedTimer0;
                      TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
                      bMute = 0;
                    }
                    else if ((iTx>5) && (iTx<80) && (iTy>12) && (iTy<75))      // XEX and D1 Disk Drive
                    {
                      u16 savedTimer0 = TIMER0_DATA;
                      bMute = 1;
                      swiWaitForVBlank();
                      // Find files in current directory and show it
                      keys_touch=1;
                      strcpy(file_load_id, "XEX/CAR/D1");
                      file_load_idx = 0;
                      chdir(last_working_dir[0]);
                      ucFicAct = last_file_index[0];                      
                      a8FindFiles();
                      romSel=dsWaitForRom();
                      if (romSel) 
                      { 
                          last_file_index[0] = ucFicAct;
                          getcwd(last_working_dir[0], 299);
                          emu_state=A8_PLAYINIT; 
                          dsLoadGame(a8romlist[ucFicAct].filename, (isDisk(a8romlist[ucFicAct].filename) ? DISK_1:DISK_XEX), bLoadAndBoot, last_read_only[0]); 
                          dsLoadGame(last_disk_filename[1], DISK_2,   false, last_read_only[1]);  // Mount D2 if available (must be done after XEX/CAR/D1)
                      }
                      else
                      {
                          TIMER0_CR=0;
                          TIMER0_DATA=savedTimer0;
                          TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
                          bMute = 0;
                      }
                    }
                    else if ((iTx>5) && (iTx<80) && (iTy>77) && (iTy<114))      // D2 Disk Drive
                    {
                      u16 savedTimer0 = TIMER0_DATA;
                      bMute = 1;
                      swiWaitForVBlank();
                      // Find files in current directory and show it
                      keys_touch=1;
                      strcpy(file_load_id, "D2");
                      file_load_idx = 1;
                      chdir(last_working_dir[1]);
                      ucFicAct = last_file_index[1];
                      a8FindFiles();
                      romSel=dsWaitForRom();
                      if (romSel)
                      { 
                          last_file_index[1] = ucFicAct;
                          getcwd(last_working_dir[1], 299);
                          emu_state=A8_PLAYINIT; 
                          dsLoadGame(a8romlist[ucFicAct].filename, DISK_2, false, last_read_only[1]); 
                      }
                      else {
                           TIMER0_CR=0;
                           TIMER0_DATA=savedTimer0;
                           TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
                           bMute = 0;
                        }
                    }
                 }
            }
        }
        else
        {
            last_key_code = -1;
            keys_touch=0;
        }

        // ---------------------------------------------------------------------------
        // Handle any of the 8 possible keys... ABXYLR and START, SELECT
        // ---------------------------------------------------------------------------
        u8 joy1_fired = false; u8 joy2_fired = false;
        u8 joy1_moved[4] = {0,0,0,0};   // Up, Down, Left, Right - Joystick 1
        u8 joy2_moved[4] = {0,0,0,0};   // Up, Down, Left, Right - Joystick 2
        for (int i=0; i<8; i++)
        {
            if (keys_pressed & nds_keys[i]) // Is this key pressed?
            {
                switch (myConfig.keyMap[i])
                {
                    case 0: joy1_fired = true;              break;
                    case 1: joy1_moved[0] = true;           break;
                    case 2: joy1_moved[1] = true;           break;
                    case 3: joy1_moved[2] = true;           break;
                    case 4: joy1_moved[3] = true;           break;
                    case 5: joy2_fired = true;              break;
                    case 6: joy2_moved[0] = true;           break;
                    case 7: joy2_moved[1] = true;           break;
                    case 8: joy2_moved[2] = true;           break;
                    case 9: joy2_moved[3] = true;           break;

                    case 10: key_consol &= ~CONSOL_START;   break;
                    case 11: key_consol &= ~CONSOL_SELECT;  break;
                    case 12: key_consol &= ~CONSOL_OPTION;  break;
                    case 13: key_code = AKEY_HELP;          break;

                    case 14:  key_code = AKEY_SPACE;        break;
                    case 15:  key_code = AKEY_RETURN;       break;
                    case 16: key_code = AKEY_ESCAPE;        break;
                    case 17: key_code = AKEY_BREAK;         break;
                    case 18: key_code = AKEY_A;             break;
                    case 19: key_code = AKEY_B;             break;
                    case 20: key_code = AKEY_C;             break;
                    case 21: key_code = AKEY_D;             break;
                    case 22: key_code = AKEY_E;             break;
                    case 23: key_code = AKEY_F;             break;
                    case 24: key_code = AKEY_G;             break;
                    case 25: key_code = AKEY_H;             break;
                    case 26: key_code = AKEY_I;             break;
                    case 27: key_code = AKEY_J;             break;
                    case 28: key_code = AKEY_K;             break;
                    case 29: key_code = AKEY_L;             break;
                    case 30: key_code = AKEY_M;             break;
                    case 31: key_code = AKEY_N;             break;
                    case 32: key_code = AKEY_O;             break;
                    case 33: key_code = AKEY_P;             break;
                    case 34: key_code = AKEY_Q;             break;
                    case 35: key_code = AKEY_R;             break;
                    case 36: key_code = AKEY_S;             break;
                    case 37: key_code = AKEY_T;             break;
                    case 38: key_code = AKEY_U;             break;
                    case 39: key_code = AKEY_V;             break;
                    case 40: key_code = AKEY_W;             break;
                    case 41: key_code = AKEY_X;             break;
                    case 42: key_code = AKEY_Y;             break;
                    case 43: key_code = AKEY_Z;             break;
                    case 44: key_code = AKEY_0;             break;
                    case 45: key_code = AKEY_1;             break;
                    case 46: key_code = AKEY_2;             break;
                    case 47: key_code = AKEY_3;             break;
                    case 48: key_code = AKEY_4;             break;
                    case 49: key_code = AKEY_5;             break;
                    case 50: key_code = AKEY_6;             break;
                    case 51: key_code = AKEY_7;             break;
                    case 52: key_code = AKEY_8;             break;
                    case 53: key_code = AKEY_9;             break;
                    case 54: key_code = AKEY_UP;            break;
                    case 55: key_code = AKEY_DOWN;          break;
                    case 56: key_code = AKEY_LEFT;          break;
                    case 57: key_code = AKEY_RIGHT;         break;
                    case 58: key_code = AKEY_NONE;          break;
                    case 59: key_code = AKEY_NONE;          break;
                    case 60: key_code = AKEY_NONE;          break;

                    case 61: screen_slide_y = 12;  dampen_slide_y = 6;     break;
                    case 62: screen_slide_y = 24;  dampen_slide_y = 6;     break;
                    case 63: screen_slide_y = -12; dampen_slide_y = 6;     break;
                    case 64: screen_slide_y = -24; dampen_slide_y = 6;     break;
                    case 65: screen_slide_x = 32;  dampen_slide_x = 6;     break;
                    case 66: screen_slide_x = 64;  dampen_slide_x = 6;     break;
                    case 67: screen_slide_x = -32; dampen_slide_x = 6;     break;
                    case 68: screen_slide_x = -64; dampen_slide_x = 6;     break;
                    case 69:
                        if (gTotalAtariFrames & 1)  // Every other frame...
                        {
                            if ((keys_pressed & KEY_UP))    myConfig.yOffset++;
                            if ((keys_pressed & KEY_DOWN))  myConfig.yOffset--;
                            if ((keys_pressed & KEY_LEFT))  myConfig.xOffset++;
                            if ((keys_pressed & KEY_RIGHT)) myConfig.xOffset--;
                        }
                        break;
                    case 70:
                        if (gTotalAtariFrames & 1)  // Every other frame...
                        {
                            if ((keys_pressed & KEY_UP))     if (myConfig.yScale <= 256) myConfig.yScale++;
                            if ((keys_pressed & KEY_DOWN))   if (myConfig.yScale >= 192) myConfig.yScale--;
                            if ((keys_pressed & KEY_RIGHT))  if (myConfig.xScale < 320)  myConfig.xScale++;
                            if ((keys_pressed & KEY_LEFT))   if (myConfig.xScale >= 192) myConfig.xScale--;
                        }
                        break;
                    case 71:
                        dsZoomScreen();
                        break;
                }
            }
        }

        // ---------------------------------------------------------------------------------------------
        // Handle the NDS D-Pad which usually just controlls a joystick connected to the Player 1 PORT.
        // Only handle UP/DOWN/LEFT/RIGHT if shoulder buttons are not pressed (those are handled below)
        // ---------------------------------------------------------------------------------------------
        if (myConfig.dpad_type == 2) // Diagonals (for Q-Bert like games - only supported on Joystick 1)
        {
            if (keys_pressed & KEY_UP)      {joy1_moved[0] = true; joy1_moved[3] = true;}
            if (keys_pressed & KEY_LEFT)    {joy1_moved[0] = true; joy1_moved[2] = true;}
            if (keys_pressed & KEY_RIGHT)   {joy1_moved[1] = true; joy1_moved[3] = true;}
            if (keys_pressed & KEY_DOWN)    {joy1_moved[1] = true; joy1_moved[2] = true;}
        }
        else if (myConfig.dpad_type == 3) // Cursor Keys
        {
            if (keys_pressed & KEY_UP)      {key_code = AKEY_UP;}
            if (keys_pressed & KEY_LEFT)    {key_code = AKEY_LEFT;}
            if (keys_pressed & KEY_RIGHT)   {key_code = AKEY_RIGHT;}
            if (keys_pressed & KEY_DOWN)    {key_code = AKEY_DOWN;}
        }
        else if (myConfig.dpad_type == 1) // Joystick Player 2
        {
            if (keys_pressed & KEY_UP)      {joy2_moved[0] = true;}
            if (keys_pressed & KEY_DOWN)    {joy2_moved[1] = true;}
            if (keys_pressed & KEY_LEFT)    {joy2_moved[2] = true;}
            if (keys_pressed & KEY_RIGHT)   {joy2_moved[3] = true;}
        }
        else                              // Joystick Player 1 is the default
        {
            if (keys_pressed & KEY_UP)      {joy1_moved[0] = true;}
            if (keys_pressed & KEY_DOWN)    {joy1_moved[1] = true;}
            if (keys_pressed & KEY_LEFT)    {joy1_moved[2] = true;}
            if (keys_pressed & KEY_RIGHT)   {joy1_moved[3] = true;}
        }

        // --------------------------------------------------------------------------
        // If any DS key resulted in the fire button being pressed, handle that here
        // --------------------------------------------------------------------------
        trig0 = (joy1_fired ? 0 : 1);
        trig1 = (joy2_fired ? 0 : 1);

        // -----------------------------------------------------------------------
        // If any DS key resulted in a joystick being moved, handle that here
        // joy_moved[] array is: Up, Down, Left, Right
        // -----------------------------------------------------------------------
        if      (joy1_moved[0] && joy1_moved[2])   stick0 = STICK_UL;
        else if (joy1_moved[0] && joy1_moved[3])   stick0 = STICK_UR;
        else if (joy1_moved[1] && joy1_moved[2])   stick0 = STICK_LL;
        else if (joy1_moved[1] && joy1_moved[3])   stick0 = STICK_LR;
        else if (joy1_moved[0])                    stick0 = STICK_FORWARD;
        else if (joy1_moved[1])                    stick0 = STICK_BACK;
        else if (joy1_moved[2])                    stick0 = STICK_LEFT;
        else if (joy1_moved[3])                    stick0 = STICK_RIGHT;

        if      (joy2_moved[0] && joy2_moved[2])   stick1 = STICK_UL;
        else if (joy2_moved[0] && joy2_moved[3])   stick1 = STICK_UR;
        else if (joy2_moved[1] && joy2_moved[2])   stick1 = STICK_LL;
        else if (joy2_moved[1] && joy2_moved[3])   stick1 = STICK_LR;
        else if (joy2_moved[0])                    stick1 = STICK_FORWARD;
        else if (joy2_moved[1])                    stick1 = STICK_BACK;
        else if (joy2_moved[2])                    stick1 = STICK_LEFT;
        else if (joy2_moved[3])                    stick1 = STICK_RIGHT;

        // ----------------------------------------------------------------------
        // This is stuff that happens more rarely... like once per frame or two.
        // ----------------------------------------------------------------------
        if (gTotalAtariFrames & 1)  // Every other frame...
        {
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_L))
            {
                if (++config_snap_counter == 20)
                {
                    if (keys_pressed & KEY_A)
                    {
                        lcdSwap();  // Exchange (Swap) LCD screens...
                    }
                    else
                    {
                        dsPrintValue(3,0,0, (char*)"SNAP");
                        screenshot();
                        dsPrintValue(3,0,0, (char*)"    ");
                    }
                }
            } else config_snap_counter=0;
        }

        break;
    }
  }
}

//----------------------------------------------------------------------------------
// Useful qsort() comparision routine to quickly sort the files found on SD card.
// We always prioritize directories first so they show at the top of the listing...
//----------------------------------------------------------------------------------
int a8Filescmp (const void *c1, const void *c2) {
  FICA_A8 *p1 = (FICA_A8 *) c1;
  FICA_A8 *p2 = (FICA_A8 *) c2;
  if (p1->filename[0] == '.' && p2->filename[0] != '.')
      return -1;
  if (p2->filename[0] == '.' && p1->filename[0] != '.')
      return 1;
  if (p1->directory && !(p2->directory))
      return -1;
  if (p2->directory && !(p1->directory))
      return 1;
  return strcasecmp (p1->filename, p2->filename);
}


//----------------------------------------------------------------------------------
// Find files game available. We sort them directories first and then alphabetical.
//----------------------------------------------------------------------------------
void a8FindFiles(void)
{
  static bool bFirstTime = true;
  DIR *pdir;
  struct dirent *pent;
  static char filenametmp[300];

  if (a8romlist == NULL) a8romlist = malloc(MAX_FILES * sizeof(FICA_A8));

  count8bit = countfiles= 0;

  // First time load... get into the root directory for easy navigation...
  if (bFirstTime)
  {
    bFirstTime = false;
  }
  pdir = opendir(".");

  if (pdir) {

    while (((pent=readdir(pdir))!=NULL))
    {
      if (count8bit > (MAX_FILES-1)) break;
      strcpy(filenametmp,pent->d_name);
      if (pent->d_type == DT_DIR)
      {
        if (!( (filenametmp[0] == '.') && (strlen(filenametmp) == 1))) {
          a8romlist[count8bit].directory = true;
          strcpy(a8romlist[count8bit].filename,filenametmp);
          count8bit++;
        }
      }
      else
      {
          if (strcmp(file_load_id,"D2")!=0)      // For D2: we don't load .xex
          {
              if ( (strcasecmp(strrchr(filenametmp, '.'), ".xex") == 0) )  {
                a8romlist[count8bit].directory = false;
                strcpy(a8romlist[count8bit].filename,filenametmp);
                count8bit++;countfiles++;
              }
              if ( (strcasecmp(strrchr(filenametmp, '.'), ".car") == 0) )  {
                a8romlist[count8bit].directory = false;
                strcpy(a8romlist[count8bit].filename,filenametmp);
                count8bit++;countfiles++;
              }
              if ( (strcasecmp(strrchr(filenametmp, '.'), ".rom") == 0) )  {
                a8romlist[count8bit].directory = false;
                strcpy(a8romlist[count8bit].filename,filenametmp);
                count8bit++;countfiles++;
              }
          }
          if ( (strcasecmp(strrchr(filenametmp, '.'), ".atr") == 0) )  {
            a8romlist[count8bit].directory = false;
            strcpy(a8romlist[count8bit].filename,filenametmp);
            count8bit++;countfiles++;
          }
          if ( (strcasecmp(strrchr(filenametmp, '.'), ".atx") == 0) )  {
            a8romlist[count8bit].directory = false;
            strcpy(a8romlist[count8bit].filename,filenametmp);
            count8bit++;countfiles++;
          }
      }
    }
    closedir(pdir);
  }
  if (count8bit)
  {
    qsort (a8romlist, count8bit, sizeof (FICA_A8), a8Filescmp);
  }
  else  // Failsafe... always provide a back directory...
  {
    a8romlist[count8bit].directory = true;
    strcpy(a8romlist[count8bit].filename,"..");
    count8bit = 1;
  }
}

void _putchar(char character) {};   // Not used but needed to link printf()

// End of file

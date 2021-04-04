/*
 * MAIN.C contains the entry point for the ARM9 execution.
 *
 * XEGS-DS - Atari 8-bit Emulator designed to run 8-bit games on the Nitendo DS/DSi
 * Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */
#ifndef _MAIN_H
#define _MAIN_H

#include <nds.h>

#define EMU_INIT       1
#define EMU_END        2

extern int bg0, bg1, bg2, bg3;             // BG pointers 
extern int bg0s, bg1s, bg2s, bg3s;         // sub BG pointers 

extern volatile u16 vusCptVBL;             // VBL test

extern u8 trig0, trig1;
extern u8 stick0, stick1;
extern int skip_frames;

#endif

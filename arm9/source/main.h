/*
 * MAIN.C contains the entry point for the ARM9 execution.
 *
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)

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

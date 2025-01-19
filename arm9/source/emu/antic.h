/*
 * ANTIC.C contains the emulation of the ANTIC chip on the Atari 800. 
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
 * antic.h - ANTIC chip emulation
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#ifndef _ANTIC_H_
#define _ANTIC_H_

#include "atari.h"

/*
 * Offset to registers in custom relative to start of antic memory addresses.
 */

#define _DMACTL     0x00
#define _CHACTL     0x01
#define _DLISTL     0x02
#define _DLISTH     0x03
#define _HSCROL     0x04
#define _VSCROL     0x05
#define _PMBASE     0x07
#define _CHBASE     0x09
#define _WSYNC      0x0a
#define _VCOUNT     0x0b
#define _PENH       0x0c
#define _PENV       0x0d
#define _NMIEN      0x0e
#define _NMIRES     0x0f
#define _NMIST      0x0f

/* pointer to a function that draws a single line of graphics */
typedef void (*draw_antic_function)(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr);
typedef void (*draw_antic_0_function)(void);

extern draw_antic_function draw_antic_ptr;
extern void (*draw_antic_0_ptr)(void);

extern UBYTE ANTIC_memory[52];
extern UBYTE DMACTL;
extern UBYTE CHACTL;
extern UWORD dlist;
extern UBYTE HSCROL;
extern UBYTE VSCROL;
extern UBYTE PMBASE;
extern UBYTE CHBASE;
extern UBYTE NMIEN;
extern UBYTE NMIST;
extern UWORD *scrn_ptr;
extern const UBYTE *antic_xe_ptr;
extern int break_ypos;
extern int ypos;
extern UBYTE wsync_halt;
extern unsigned int screenline_cpu_clock;
extern UBYTE PENH_input;
extern UBYTE PENV_input;

#define ANTIC_CPU_CLOCK (screenline_cpu_clock + xpos)
#define ANTIC_xpos xpos
#define ANTIC_ypos ypos

#define NMIST_C 6
#define NMI_C   12


#ifdef NEW_CYCLE_EXACT
#define NOT_DRAWING -999
#define DRAWING_SCREEN (cur_screen_pos!=NOT_DRAWING)
extern int *cpu2antic_ptr;
extern int *antic2cpu_ptr;
extern UBYTE delayed_wsync;
extern UBYTE dmactl_changed;
extern UBYTE DELAYED_DMACTL;
extern UBYTE draw_antic_ptr_changed;
extern UBYTE need_load;
extern int dmactl_bug_chdata;
extern int cur_screen_pos;
void update_scanline(void);
void update_scanline_prior(UBYTE byte);
#ifndef NO_GTIA11_DELAY
extern int prevline_prior_pos;
extern int curline_prior_pos;
extern int prior_curpos;
#define PRIOR_BUF_SIZE 40
extern UBYTE prior_val_buf[PRIOR_BUF_SIZE];
extern short int prior_pos_buf[PRIOR_BUF_SIZE];
#endif /* NO_GTIA11_DELAY */

#define XPOS ( DRAWING_SCREEN ? cpu2antic_ptr[xpos] : xpos )
#else
#define XPOS xpos
#endif /* NEW_CYCLE_EXACT */

extern UBYTE PENH;
extern UBYTE PENV;
extern UWORD screenaddr;
extern UBYTE IR;
extern UBYTE anticmode;
extern UBYTE dctr;
extern UBYTE lastline;
extern UBYTE need_dl;
extern UBYTE vscrol_off;
extern UBYTE md;
extern UBYTE chars_read[6];
extern UBYTE  chars_displayed[6];
extern short int x_min[6];
extern short int ch_offset[6];
extern short int load_cycles[6];
extern short int font_cycles[6];
extern short int before_cycles[6];
extern short int extra_cycles[6];
extern short int left_border_chars;
extern short int right_border_start;
extern UWORD chbase_20;
extern UBYTE invert_mask;
extern UBYTE blank_mask;
extern UBYTE an_scanline[ATARI_WIDTH / 2 + 8];
extern UBYTE blank_lookup[256];
extern UWORD lookup2[256];
extern ULONG lookup_gtia9[16];
extern ULONG lookup_gtia11[16];
extern UBYTE playfield_lookup[257];
extern UBYTE mode_e_an_lookup[256];
extern UWORD cl_lookup[128];
extern UWORD hires_lookup_n[128];
extern UWORD hires_lookup_m[128];
extern UWORD hires_lookup_l[128];
extern UBYTE singleline;
extern UBYTE player_dma_enabled;
extern UBYTE player_gra_enabled;
extern UBYTE missile_dma_enabled;
extern UBYTE missile_gra_enabled;
extern UBYTE player_flickering;
extern UBYTE missile_flickering;
extern UWORD pmbase_s;
extern UWORD pmbase_d;
extern UBYTE pm_scanline[ATARI_WIDTH / 2 + 8];
extern UBYTE pm_dirty;
extern const UBYTE *pm_lookup_ptr;


void ANTIC_Initialise(void);
void ANTIC_Reset(void);
void ANTIC_Frame(int draw_display);
UBYTE ANTIC_GetByte(UWORD addr);
void ANTIC_PutByte(UWORD addr, UBYTE byte);
UBYTE ANTIC_GetDLByte(UWORD *paddr);
UWORD ANTIC_GetDLWord(UWORD *paddr);
void ANTIC_UpdateArtifacting(void);
UBYTE get_antic_function_idx(void);
void set_antic_function_by_idx(UBYTE idx);
UBYTE get_antic_0_function_idx(void);
void set_antic_0_function_by_idx(UBYTE idx);

/* Video memory access */
void video_memset(UBYTE *ptr, UBYTE val, ULONG size);
void video_putbyte(UBYTE *ptr, UBYTE val);

#endif /* _ANTIC_H_ */

/*
 * ANTIC.C contains the emulation of the ANTIC chip on the Atari 800. 
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly ecommended that you seek out the latest Atari800 sources.
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
 * be distributed using that same licensing model. See COPYING for the full license
 * but the original Atari800 copyright notice retained below:
 */


/*
 * antic.c - ANTIC chip emulation
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
#include <string.h>
#include <nds.h>

#include "a8ds.h"
#include "antic.h"
#include "atari.h"
#include "cpu.h"
#include "gtia.h"
#include "memory.h"
#include "pokeysnd.h"
#include "util.h"
#include "input.h"

#define LCHOP 3         /* do not build lefmost 0..3 characters in wide mode */
#define RCHOP 3         /* do not build rightmost 0..3 characters in wide mode */

int break_ypos __attribute__((section(".dtcm"))) = 999;

/* Video memory access is hidden behind these macros. It allows to track dirty video memory
   to improve video system performance */

#define WRITE_VIDEO(ptr, val)       (*(ptr) = val)
#define WRITE_VIDEO_LONG(ptr, val)  (*(ptr) = val)
#define WRITE_VIDEO_BYTE(ptr, val)  (*(ptr) = val)
#define FILL_VIDEO(ptr, val, size)  memset(ptr, val, size)
#define READ_VIDEO_LONG(ptr)        (*(ptr))


/* Memory access helpers----------------------------------------------------- */
/* Some optimizations result in unaligned 32-bit accesses. These macros have
   been introduced for machines that don't allow unaligned memory accesses. */

#define WRITE_VIDEO_LONG_UNALIGNED(ptr, val)  UNALIGNED_PUT_LONG((ptr), (val))

#define IS_ZERO_ULONG(x) (((ULONG)x & 0x03) ? (!((const UBYTE *)(x))[0] && !((const UBYTE *)(x))[1] && !((const UBYTE *)(x))[2] && !((const UBYTE *)(x))[3]) : (! UNALIGNED_GET_LONG(x)))

#define DO_GTIA_BYTE(p, l, x) { \
        if (((ULONG)p & 0x03) == 0) { \
          UNALIGNED_PUT_LONG((ULONG *) (p),     (l)[(x) >> 4]); \
          UNALIGNED_PUT_LONG((ULONG *) (p) + 1, (l)[(x) & 0xf]); \
        } else { \
          WRITE_VIDEO((UWORD *) (p),     (UWORD) ((l)[(x) >> 4])); \
          WRITE_VIDEO((UWORD *) (p) + 1, (UWORD) ((l)[(x) >> 4])); \
          WRITE_VIDEO((UWORD *) (p) + 2, (UWORD) ((l)[(x) & 0xf])); \
          WRITE_VIDEO((UWORD *) (p) + 3, (UWORD) ((l)[(x) & 0xf]));  \
        } \
    }


/* ANTIC Memory ------------------------------------------------------------ */

UBYTE ANTIC_memory[52] __attribute__((section(".dtcm")));
#define ANTIC_margin 4
/* It's number of bytes in ANTIC_memory, which are never loaded, but may be
   read in wide playfield mode. These bytes are uninitialized, because on
   real computer there's some kind of 'garbage'. Possibly 1 is enough, but
   4 bytes surely won't cause negative indexes. :) */

/* ANTIC Registers --------------------------------------------------------- */

UBYTE DMACTL __attribute__((section(".dtcm")));
UBYTE CHACTL __attribute__((section(".dtcm")));
UWORD dlist __attribute__((section(".dtcm")));
UBYTE HSCROL __attribute__((section(".dtcm")));
UBYTE VSCROL __attribute__((section(".dtcm")));
UBYTE PMBASE __attribute__((section(".dtcm")));
UBYTE CHBASE __attribute__((section(".dtcm")));
UBYTE NMIEN __attribute__((section(".dtcm")));
UBYTE NMIST __attribute__((section(".dtcm")));

/* Screen -----------------------------------------------------------------
   Define screen as ULONG to ensure that it is Longword aligned.
   This allows special optimisations under certain conditions.
   ------------------------------------------------------------------------ */

UWORD *scrn_ptr __attribute__((section(".dtcm")));

/* Separate access to XE extended memory ----------------------------------- */
/* It's available in 130 XE and 576 KB Compy Shop.
   Note: during ANTIC access to extended memory in Compy Shop Self Test
   is disabled. It is unknown if this is true for real 130 XE. If not,
   then some extra code has to be added to:
   - check if selftest_enabled is set
   - check if the address is in range 0x5000..0x57ff
   - if both conditions are true, then access memory instead of antic_xe_ptr */

/* Pointer to 16 KB seen by ANTIC in 0x4000-0x7fff.
   If it's the same what the CPU sees (and what's in memory[0x4000..0x7fff],
   then NULL. */
const UBYTE *antic_xe_ptr __attribute__((section(".dtcm")))= NULL;

/* ANTIC Timing --------------------------------------------------------------

I've introduced global variable xpos, which contains current number of cycle
in a line. This simplifies ANTIC/CPU timing much. The GO() function which
emulates CPU is now void and is called with xpos limit, below which CPU can go.

All strange variables holding 'unused cycles', 'DMA cycles', 'allocated cycles'
etc. are removed. Simply whenever ANTIC fetches a byte, it takes single cycle,
which can be done now with xpos++. There's only one exception: in text modes
2-5 ANTIC takes more bytes than cycles, because it does less than DMAR refresh
cycles.

Now emulation is really screenline-oriented. We do ypos++ after a line,
not inside it.

This simplified diagram shows when what is done in a line:

MDPPPPDD..............(------R/S/F------)..........
^  ^     ^      ^     ^                     ^    ^ ^        ---> time/xpos
0  |  NMIST_C NMI_C SCR_C                 WSYNC_C|LINE_C
VSCON_C                                        VSCOF_C

M - fetch Missiles
D - fetch DL
P - fetch Players
S - fetch Screen
F - fetch Font (in text modes)
R - refresh Memory (DMAR cycles)

Only Memory Refresh happens in every line, other tasks are optional.

Below are exact diagrams for some non-scrolled modes:
                                                                                                    11111111111111
          11111111112222222222333333333344444444445555555555666666666677777777778888888888999999999900000000001111
012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123
                            /--------------------------narrow------------------------------\
                    /----------------------------------normal--------------------------------------\
            /-------------------------------------------wide--------------------------------------------\

blank line:
MDPPPPDD.................R...R...R...R...R...R...R...R...R........................................................

mode 8,9:
MDPPPPDD....S.......S....R..SR...R..SR...R..SR...R..SR...R..S.......S.......S.......S.......S.......S.............

mode a,b,c:
MDPPPPDD....S...S...S...SR..SR..SR..SR..SR..SR..SR..SR..SR..S...S...S...S...S...S...S...S...S...S...S...S.........

mode d,e,f:
MDPPPPDD....S.S.S.S.S.S.SRS.SRS.SRS.SRS.SRS.SRS.SRS.SRS.SRS.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.S.........

Notes:
* At the beginning of a line fetched are:
  - a byte of Missiles
  - a byte of DL (instruction)
  - four bytes of Players
  - two bytes of DL argument (jump or screen address)
  The emulator, however, fetches them all continuously.

* Refresh cycles and Screen/Font fetches have been tested for some modes (see above).
  This is for making the emulator more accurate, able to change colour registers,
  sprite positions or GTIA modes during scanline. These modes are the most commonly used
  with those effects.
  Currently this isn't implemented, and all R/S/F cycles are fetched continuously in *all* modes
  (however, right number of cycles is taken in every mode, basing on screen width and HSCROL).

There are a few constants representing following events:

* VSCON_C - in first VSC line dctr is loaded with VSCROL

* NMIST_C - NMIST is updated (set to 0x9f on DLI, set to 0x5f on VBLKI)

* NMI_C - If NMIEN permits, NMI interrupt is generated

* SCR_C - We draw whole line of screen. On a real computer you can change
  ANTIC/GTIA registers while displaying screen, however this emulator
  isn't that accurate.

* WSYNC_C - ANTIC holds CPU until this moment, when WSYNC is written

* VSCOF_C - in last VSC line dctr is compared with VSCROL

* LINE_C - simply end of line (this used to be called CPUL)

All constants are determined by tests on real Atari computer. It is assumed,
that ANTIC registers are read with LDA, LDX, LDY and written with STA, STX,
STY, all in absolute addressing mode. All these instructions last 4 cycles
and perform read/write operation in last cycle. The CPU emulation should
correctly emulate WSYNC and add cycles for current instruction BEFORE
executing it. That's why VSCOF_C > LINE_C is correct.

How WSYNC is now implemented:

* On writing WSYNC:
  - if xpos <= WSYNC_C && xpos_limit >= WSYNC_C,
    we only change xpos to WSYNC_C - that's all
  - otherwise we set wsync_halt and change xpos to xpos_limit causing GO()
    to return

* At the beginning of GO() (CPU emulation), when wsync_halt is set:
  - if xpos_limit < WSYNC_C we return
  - else we set xpos to WSYNC_C, reset wsync_halt and emulate some cycles

We don't emulate NMIST_C, NMI_C and SCR_C if it is unnecessary.
These are all cases:

* Common overscreen line
  Nothing happens except that ANTIC gets DMAR cycles:
  xpos += DMAR; GOEOL;

* First overscreen line - start of vertical blank
  - CPU goes until NMIST_C
  - ANTIC sets NMIST to 0x5f
  if (NMIEN & 0x40) {
      - CPU goes until NMI_C
      - ANTIC forces NMI
  }
  - ANTIC gets DMAR cycles
  - CPU goes until LINE_C

* Screen line without DLI
  - ANTIC fetches DL and P/MG
  - CPU goes until SCR_C
  - ANTIC draws whole line fetching Screen/Font and refreshing memory
  - CPU goes until LINE_C

* Screen line with DLI
  - ANTIC fetches DL and P/MG
  - CPU goes until NMIST_C
  - ANTIC sets NMIST to 0x9f
  if (NMIEN & 0x80) {
      - CPU goes until NMI_C
      - ANTIC forces NMI
  }
  - CPU goes until SCR_C
  - ANTIC draws line with DMAR
  - CPU goes until LINE_C

  -------------------------------------------------------------------------- */

#define VSCON_C 1
#define SCR_C   28
#define VSCOF_C 112

unsigned int screenline_cpu_clock __attribute__((section(".dtcm"))) = 0;

#define UPDATE_DMACTL

#define GOEOL_CYCLE_EXACT  GO(antic2cpu_ptr[LINE_C]); \
    xpos = cpu2antic_ptr[xpos]; \
    xpos -= LINE_C; \
    screenline_cpu_clock += LINE_C; \
    ypos++; \
    update_pmpl_colls();
#define GOEOL GO(LINE_C); xpos -= LINE_C; screenline_cpu_clock += LINE_C; UPDATE_DMACTL ypos++
#define OVERSCREEN_LINE xpos += DMAR; GOEOL

int xpos __attribute__((section(".dtcm"))) = 0;
int xpos_limit __attribute__((section(".dtcm")));
UBYTE wsync_halt __attribute__((section(".dtcm"))) = FALSE;

int ypos __attribute__((section(".dtcm")));                     /* Line number - lines 8..247 are on screen */

/* Timing in first line of modes 2-5
In these modes ANTIC takes more bytes than cycles. Despite this, it would be
possible that SCR_C + cycles_taken > WSYNC_C. To avoid this we must take some
cycles before SCR_C. before_cycles contains number of them, while extra_cycles
contains difference between bytes taken and cycles taken plus before_cycles. */

#define BEFORE_CYCLES (SCR_C - 28)
/* It's number of cycles taken before SCR_C for not scrolled, narrow playfield.
   It wasn't tested, but should be ok. ;) */

/* Light pen support ------------------------------------------------------- */

UBYTE PENH __attribute__((section(".dtcm")));
UBYTE PENV __attribute__((section(".dtcm")));
UBYTE PENH_input __attribute__((section(".dtcm")))= 0x00;
UBYTE PENV_input __attribute__((section(".dtcm")))= 0xff;

/* Internal ANTIC registers ------------------------------------------------ */

UWORD screenaddr __attribute__((section(".dtcm")));      /* Screen Pointer */
UBYTE IR __attribute__((section(".dtcm")));              /* Instruction Register */
UBYTE anticmode __attribute__((section(".dtcm")));       /* Antic mode */
UBYTE dctr __attribute__((section(".dtcm")));            /* Delta Counter */
UBYTE lastline __attribute__((section(".dtcm")));        /* dctr limit */
UBYTE need_dl __attribute__((section(".dtcm")));         /* boolean: fetch DL next line */
UBYTE vscrol_off __attribute__((section(".dtcm")));      /* boolean: displaying line ending VSC */

/* Pre-computed values for improved performance ---------------------------- */

#define NORMAL0 0               /* modes 2,3,4,5,0xd,0xe,0xf */
#define NORMAL1 1               /* modes 6,7,0xa,0xb,0xc */
#define NORMAL2 2               /* modes 8,9 */
#define SCROLL0 3               /* modes 2,3,4,5,0xd,0xe,0xf with HSC */
#define SCROLL1 4               /* modes 6,7,0xa,0xb,0xc with HSC */
#define SCROLL2 5               /* modes 8,9 with HSC */
int md __attribute__((section(".dtcm")));                    /* current mode NORMAL0..SCROLL2 */
/* tables for modes NORMAL0..SCROLL2 */
int chars_read[6] __attribute__((section(".dtcm")));
int chars_displayed[6] __attribute__((section(".dtcm")));
int x_min[6] __attribute__((section(".dtcm")));
int ch_offset[6] __attribute__((section(".dtcm")));
int load_cycles[6] __attribute__((section(".dtcm")));
int font_cycles[6] __attribute__((section(".dtcm")));
int before_cycles[6] __attribute__((section(".dtcm")));
int extra_cycles[6] __attribute__((section(".dtcm")));

/* border parameters for current display width */
int left_border_chars __attribute__((section(".dtcm")));
int right_border_start __attribute__((section(".dtcm")));
#define LBORDER_START (LCHOP * 4)
#define RBORDER_END ((48 - RCHOP) * 4)

/* set with CHBASE *and* CHACTL - bits 0..2 set if flip on */
UWORD chbase_20 __attribute__((section(".dtcm")));           /* CHBASE for 20 character mode */

/* set with CHACTL */
UBYTE invert_mask __attribute__((section(".dtcm")));
int blank_mask __attribute__((section(".dtcm")));

/* A scanline of AN0 and AN1 signals as transmitted from ANTIC to GTIA.
   In every byte, bit 0 is AN0 and bit 1 is AN1 */
UBYTE an_scanline[ATARI_WIDTH / 2 + 8] __attribute__((section(".dtcm")));

/* lookup tables */
UBYTE blank_lookup[256] __attribute__((section(".dtcm")));
UWORD lookup2[256] __attribute__((section(".dtcm")));
ULONG lookup_gtia9[16] __attribute__((section(".dtcm")));
ULONG lookup_gtia11[16] __attribute__((section(".dtcm")));
UBYTE playfield_lookup[257] __attribute__((section(".dtcm")));
UBYTE mode_e_an_lookup[256] __attribute__((section(".dtcm")));

/* Colour lookup table
   This single table replaces 4 previously used: cl_word, cur_prior,
   prior_table and pf_colls. It should be treated as a two-dimensional table,
   with playfield colours in rows and PMG colours in columns:
       no_PMG PM0 PM1 PM01 PM2 PM3 PM23 PM023 PM123 PM0123 PM25 PM35 PM235 colls ... ...
   BAK
   ...
   HI2
   HI3
   PF0
   PF1
   PF2
   PF3
   The table contains word value (lsb = msb) of colour to be drawn.
   The table is being updated taking current PRIOR setting into consideration.
   '...' represent two unused columns and single unused row.
   HI2 and HI3 are used only if colour_translation_table is being used.
   They're colours of hi-res pixels on PF2 and PF3 respectively (PF2 is
   default background for hi-res, PF3 is PM5).
   Columns PM023, PM123 and PM0123 are used when PRIOR & 0xf equals any
   of 5,7,0xc,0xd,0xe,0xf. The columns represent PM0, PM1 and PM01 respectively
   covered by PM2 and/or PM3. This is to handle black colour on PF2 and PF3.
   Columns PM25, PM35 and PM235 are used when PRIOR & 0x1f equals any
   of 0x10,0x1a,0x1c,0x1e. The columns represent PM2, PM3 and PM23
   respectively covered by PM5. This to handle colour on PF0 and PF1:
   PF3 if (PRIOR & 0x1f) == 0x10, PF0 or PF1 otherwise.
   Additional column 'colls' holds collisions of playfields with PMG. */

UWORD cl_lookup[128] __attribute__((section(".dtcm")));

#define C_PM0   0x01
#define C_PM1   0x02
#define C_PM01  0x03
#define C_PM2   0x04
#define C_PM3   0x05
#define C_PM23  0x06
#define C_PM023 0x07
#define C_PM123 0x08
#define C_PM0123 0x09
#define C_PM25  0x0a
#define C_PM35  0x0b
#define C_PM235 0x0c
#define C_COLLS 0x0d
#define C_BAK   0x00
#define C_HI2   0x20
#define C_HI3   0x30
#define C_PF0   0x40
#define C_PF1   0x50
#define C_PF2   0x60
#define C_PF3   0x70
#define C_BLACK (C_PF3 | C_PM25)

/* these are byte-offsets in the table, so left shift for indexing word table
   has been avoided */
#define COLOUR(x) (*(UWORD *) ((UBYTE *) cl_lookup + (x) ))
#define L_PM0   (2 * C_PM0)
#define L_PM1   (2 * C_PM1)
#define L_PM01  (2 * C_PM01)
#define L_PM2   (2 * C_PM2)
#define L_PM3   (2 * C_PM3)
#define L_PM23  (2 * C_PM23)
#define L_PM023 (2 * C_PM023)
#define L_PM123 (2 * C_PM123)
#define L_PM0123 (2 * C_PM0123)
#define L_PM25  (2 * C_PM25)
#define L_PM35  (2 * C_PM35)
#define L_PM235 (2 * C_PM235)
#define L_COLLS (2 * C_COLLS)
#define L_BAK   (2 * C_BAK)
#define L_HI2   (2 * C_HI2)
#define L_HI3   (2 * C_HI3)
#define L_PF0   (2 * C_PF0)
#define L_PF1   (2 * C_PF1)
#define L_PF2   (2 * C_PF2)
#define L_PF3   (2 * C_PF3)
#define L_BLACK (2 * C_BLACK)

/* Blank areas optimizations
   Routines for most graphics modes take advantage of fact, that often
   large areas of screen are background colour. If it is possible, 8 pixels
   of background are drawn at once - with two longs or four words, if
   the platform doesn't allow unaligned long access.
   Artifacting also uses unaligned long access if it's supported. */

#define INIT_BACKGROUND_6 ULONG background = cl_lookup[C_PF2] | (((ULONG) cl_lookup[C_PF2]) << 16);
#define INIT_BACKGROUND_8 ULONG background = lookup_gtia9[0];

#define DRAW_BACKGROUND(colreg) {\
    if (((ULONG)ptr & 0x03) == 0) { \
        WRITE_VIDEO_LONG_UNALIGNED(((ULONG *) ptr), background); \
        WRITE_VIDEO_LONG_UNALIGNED(((ULONG *) ptr)+1, background); \
        ptr += 4; \
    } else \
        { \
         WRITE_VIDEO(ptr++, cl_lookup[colreg]); \
         WRITE_VIDEO(ptr++, cl_lookup[colreg]); \
         WRITE_VIDEO(ptr++, cl_lookup[colreg]); \
         WRITE_VIDEO(ptr++, cl_lookup[colreg]); \
        } \
    }

#define DRAW_ARTIF {\
        WRITE_VIDEO(ptr++, ((UWORD *) art_curtable)[(screendata_tally & 0x03fc00) >> 9]); \
        WRITE_VIDEO(ptr++, ((UWORD *) art_curtable)[((screendata_tally & 0x03fc00) >> 9) + 1]); \
        WRITE_VIDEO(ptr++, ((UWORD *) art_curtable)[(screendata_tally & 0x003fc0) >> 5]); \
        WRITE_VIDEO(ptr++, ((UWORD *) art_curtable)[((screendata_tally & 0x003fc0) >> 5) + 1]); \
    }


/* Hi-res modes optimizations
   Now hi-res modes are drawn with words, not bytes. Endianess defaults
   to little-endian. */

#define BYTE0_MASK      0x00ff
#define BYTE1_MASK      0xff00
#define HIRES_MASK_01   0xf0ff
#define HIRES_MASK_10   0xfff0
#define HIRES_LUM_01    0x0f00
#define HIRES_LUM_10    0x000f

#define hires_norm(x)   hires_lookup_n[(x) >> 1]
#define hires_mask(x)   hires_lookup_m[(x) >> 1]

UWORD hires_lookup_n[128] __attribute__((section(".dtcm")));
UWORD hires_lookup_m[128] __attribute__((section(".dtcm")));
UWORD hires_lookup_l[128] __attribute__((section(".dtcm")));    /* accessed in gtia.c */
#define hires_lum(x)    hires_lookup_l[(x) >> 1]

/* Player/Missile Graphics ------------------------------------------------- */

#define PF0PM (*(UBYTE *) &cl_lookup[C_PF0 | C_COLLS])
#define PF1PM (*(UBYTE *) &cl_lookup[C_PF1 | C_COLLS])
#define PF2PM (*(UBYTE *) &cl_lookup[C_PF2 | C_COLLS])
#define PF3PM (*(UBYTE *) &cl_lookup[C_PF3 | C_COLLS])
#define PF_COLLS(x) (((UBYTE *) &cl_lookup)[(x) + L_COLLS])

UBYTE singleline __attribute__((section(".dtcm")));
UBYTE player_dma_enabled __attribute__((section(".dtcm")));
UBYTE player_gra_enabled __attribute__((section(".dtcm")));
UBYTE missile_dma_enabled __attribute__((section(".dtcm")));
UBYTE missile_gra_enabled __attribute__((section(".dtcm")));
UBYTE player_flickering __attribute__((section(".dtcm")));
UBYTE missile_flickering __attribute__((section(".dtcm")));

UWORD pmbase_s __attribute__((section(".dtcm")));
UWORD pmbase_d __attribute__((section(".dtcm")));

extern UBYTE pm_scanline[ATARI_WIDTH / 2 + 8] __attribute__((section(".dtcm")));
extern UBYTE pm_dirty __attribute__((section(".dtcm")));

/* PMG lookup tables */
UBYTE pm_lookup_table[20][256];
/* current PMG lookup table */
const UBYTE *pm_lookup_ptr __attribute__((section(".dtcm")));

#define PL_00   0   /* 0x00,0x01,0x02,0x03,0x04,0x06,0x08,0x09,0x0a,0x0b */
#define PL_05   1   /* 0x05,0x07,0x0c,0x0d,0x0e,0x0f */
#define PL_10   2   /* 0x10,0x1a */
#define PL_11   3   /* 0x11,0x18,0x19 */
#define PL_12   4   /* 0x12 */
#define PL_13   5   /* 0x13,0x1b */
#define PL_14   6   /* 0x14,0x16 */
#define PL_15   7   /* 0x15,0x17,0x1d,0x1f */
#define PL_1c   8   /* 0x1c */
#define PL_1e   9   /* 0x1e */
#define PL_20   10  /* 0x20,0x21,0x22,0x23,0x24,0x26,0x28,0x29,0x2a,0x2b */
#define PL_25   11  /* 0x25,0x27,0x2c,0x2d,0x2e,0x2f */
#define PL_30   12  /* 0x30,0x3a */
#define PL_31   13  /* 0x31,0x38,0x39 */
#define PL_32   14  /* 0x32 */
#define PL_33   15  /* 0x33,0x3b */
#define PL_34   16  /* 0x34,0x36 */
#define PL_35   17  /* 0x35,0x37,0x3d,0x3f */
#define PL_3c   18  /* 0x3c */
#define PL_3e   19  /* 0x3e */

static const UBYTE prior_to_pm_lookup[64] = {
    PL_00, PL_00, PL_00, PL_00, PL_00, PL_05, PL_00, PL_05,
    PL_00, PL_00, PL_00, PL_00, PL_05, PL_05, PL_05, PL_05,
    PL_10, PL_11, PL_12, PL_13, PL_14, PL_15, PL_14, PL_15,
    PL_11, PL_11, PL_10, PL_13, PL_1c, PL_15, PL_1e, PL_15,
    PL_20, PL_20, PL_20, PL_20, PL_20, PL_25, PL_20, PL_25,
    PL_20, PL_20, PL_20, PL_20, PL_25, PL_25, PL_25, PL_25,
    PL_30, PL_31, PL_32, PL_33, PL_34, PL_35, PL_34, PL_35,
    PL_31, PL_31, PL_30, PL_33, PL_3c, PL_35, PL_3e, PL_35
};

static void init_pm_lookup(void) {
    static const UBYTE pm_lookup_template[10][16] = {
        /* PL_20 */
        { L_BAK, L_PM0, L_PM1, L_PM01, L_PM2, L_PM0, L_PM1, L_PM01,
        L_PM3, L_PM0, L_PM1, L_PM01, L_PM23, L_PM0, L_PM1, L_PM01 },
        /* PL_25 */
        { L_BAK, L_PM0, L_PM1, L_PM01, L_PM2, L_PM023, L_PM123, L_PM0123,
        L_PM3, L_PM023, L_PM123, L_PM0123, L_PM23, L_PM023, L_PM123, L_PM0123 },
        /* PL_30 */
        { L_PF3, L_PM0, L_PM1, L_PM01, L_PM25, L_PM0, L_PM1, L_PM01,
        L_PM35, L_PM0, L_PM1, L_PM01, L_PM235, L_PM0, L_PM1, L_PM01 },
        /* PL_31 */
        { L_PF3, L_PM0, L_PM1, L_PM01, L_PM2, L_PM0, L_PM1, L_PM01,
        L_PM3, L_PM0, L_PM1, L_PM01, L_PM23, L_PM0, L_PM1, L_PM01 },
        /* PL_32 */
        { L_PF3, L_PM0, L_PM1, L_PM01, L_PF3, L_PM0, L_PM1, L_PM01,
        L_PF3, L_PM0, L_PM1, L_PM01, L_PF3, L_PM0, L_PM1, L_PM01 },
        /* PL_33 */
        { L_PF3, L_PM0, L_PM1, L_PM01, L_BLACK, L_PM0, L_PM1, L_PM01,
        L_BLACK, L_PM0, L_PM1, L_PM01, L_BLACK, L_PM0, L_PM1, L_PM01 },
        /* PL_34 */
        { L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3,
        L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3, L_PF3 },
        /* PL_35 */
        { L_PF3, L_PF3, L_PF3, L_PF3, L_BLACK, L_BLACK, L_BLACK, L_BLACK,
        L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK, L_BLACK },
        /* PL_3c */
        { L_PF3, L_PF3, L_PF3, L_PF3, L_PM25, L_PM25, L_PM25, L_PM25,
        L_PM25, L_PM25, L_PM25, L_PM25, L_PM25, L_PM25, L_PM25, L_PM25 },
        /* PL_3e */
        { L_PF3, L_PF3, L_PF3, L_PF3, L_PM25, L_BLACK, L_BLACK, L_BLACK,
        L_PM25, L_BLACK, L_BLACK, L_BLACK, L_PM25, L_BLACK, L_BLACK, L_BLACK }
    };

    static const UBYTE multi_to_normal[] = {
        L_BAK,
        L_PM0, L_PM1, L_PM0,
        L_PM2, L_PM3, L_PM2,
        L_PM023, L_PM123, L_PM023,
        L_PM25, L_PM35, L_PM25
    };

    int i;
    int j;
    UBYTE temp;

    for (i = 0; i <= 1; i++)
        for (j = 0; j <= 255; j++) {
            pm_lookup_table[i + 10][j] = temp = pm_lookup_template[i][(j & 0xf) | (j >> 4)];
            pm_lookup_table[i][j] = temp <= L_PM235 ? multi_to_normal[temp >> 1] : temp;
        }
    for (; i <= 9; i++) {
        for (j = 0; j <= 15; j++) {
            pm_lookup_table[i + 10][j] = temp = pm_lookup_template[i < 7 ? 0 : 1][j];
            pm_lookup_table[i][j] = temp <= L_PM235 ? multi_to_normal[temp >> 1] : temp;
        }
        for (; j <= 255; j++) {
            pm_lookup_table[i + 10][j] = temp = pm_lookup_template[i][j & 0xf];
            pm_lookup_table[i][j] = temp <= L_PM235 ? multi_to_normal[temp >> 1] : temp;
        }
    }
}

static UBYTE hold_missiles_tab[16] __attribute__((section(".dtcm"))) = {
    0x00,0x03,0x0c,0x0f,0x30,0x33,0x3c,0x3f,
    0xc0,0xc3,0xcc,0xcf,0xf0,0xf3,0xfc,0xff};

static void pmg_dma(void) {
    /* VDELAY bit set == GTIA ignores PMG DMA in even lines */
    if (player_dma_enabled) {
        if (player_gra_enabled) {
            const UBYTE *base;
            if (singleline) {
                if (antic_xe_ptr != NULL && pmbase_s < 0x8000 && pmbase_s >= 0x4000)
                    base = antic_xe_ptr + pmbase_s - 0x4000 + ypos;
                else
                    base = AnticMainMemLookup(pmbase_s + ypos);
                if (ypos & 1) {
                    GRAFP0 = base[0x400];
                    GRAFP1 = base[0x500];
                    GRAFP2 = base[0x600];
                    GRAFP3 = base[0x700];
                }
                else {
                    if ((VDELAY & 0x10) == 0)
                        GRAFP0 = base[0x400];
                    if ((VDELAY & 0x20) == 0)
                        GRAFP1 = base[0x500];
                    if ((VDELAY & 0x40) == 0)
                        GRAFP2 = base[0x600];
                    if ((VDELAY & 0x80) == 0)
                        GRAFP3 = base[0x700];
                }
            }
            else {
                if (antic_xe_ptr != NULL && pmbase_d < 0x8000 && pmbase_d >= 0x4000)
                    base = antic_xe_ptr + (pmbase_d - 0x4000) + (ypos >> 1);
                else
                    base = AnticMainMemLookup(pmbase_d + (ypos >> 1));
                if (ypos & 1) {
                    GRAFP0 = base[0x200];
                    GRAFP1 = base[0x280];
                    GRAFP2 = base[0x300];
                    GRAFP3 = base[0x380];
                }
                else {
                    if ((VDELAY & 0x10) == 0)
                        GRAFP0 = base[0x200];
                    if ((VDELAY & 0x20) == 0)
                        GRAFP1 = base[0x280];
                    if ((VDELAY & 0x40) == 0)
                        GRAFP2 = base[0x300];
                    if ((VDELAY & 0x80) == 0)
                        GRAFP3 = base[0x380];
                }
            }
        }
        xpos += 4;
    }
    if (missile_dma_enabled) {
        if (missile_gra_enabled) {
            UBYTE data;
            if (antic_xe_ptr != NULL && pmbase_s < 0x8000 && pmbase_s >= 0x4000)
                data = antic_xe_ptr[singleline ? pmbase_s + ypos + 0x300 - 0x4000 : pmbase_d + (ypos >> 1) + 0x180 - 0x4000];
            else
                data = dGetByte(singleline ? pmbase_s + ypos + 0x300 : pmbase_d + (ypos >> 1) + 0x180);
            /* in odd lines load all missiles, in even only those, for which VDELAY bit is zero */
            GRAFM = ypos & 1 ? data : ((GRAFM ^ data) & hold_missiles_tab[VDELAY & 0xf]) ^ data;
        }
        xpos++;
    }
}

/* Artifacting ------------------------------------------------------------ */

static ULONG art_lookup_normal[256];
static ULONG art_lookup_reverse[256];
static ULONG art_bkmask_normal[256];
static ULONG art_lummask_normal[256];
static ULONG art_bkmask_reverse[256];
static ULONG art_lummask_reverse[256];

static ULONG *art_curtable = art_lookup_normal;
static ULONG *art_curbkmask = art_bkmask_normal;
static ULONG *art_curlummask = art_lummask_normal;

static UWORD art_normal_colpf1_save;
static UWORD art_normal_colpf2_save;
static UWORD art_reverse_colpf1_save;
static UWORD art_reverse_colpf2_save;

static void setup_art_colours(void)
{
    static UWORD *art_colpf1_save = &art_normal_colpf1_save;
    static UWORD *art_colpf2_save = &art_normal_colpf2_save;
    UWORD curlum = cl_lookup[C_PF1] & 0x0f0f;

    if (curlum != *art_colpf1_save || cl_lookup[C_PF2] != *art_colpf2_save) {
        if (curlum < (cl_lookup[C_PF2] & 0x0f0f)) {
            art_colpf1_save = &art_reverse_colpf1_save;
            art_colpf2_save = &art_reverse_colpf2_save;
            art_curtable = art_lookup_reverse;
            art_curlummask = art_lummask_reverse;
            art_curbkmask = art_bkmask_reverse;
        }
        else {
            art_colpf1_save = &art_normal_colpf1_save;
            art_colpf2_save = &art_normal_colpf2_save;
            art_curtable = art_lookup_normal;
            art_curlummask = art_lummask_normal;
            art_curbkmask = art_bkmask_normal;
        }
        if (curlum ^ *art_colpf1_save) {
            int i;
            ULONG new_colour = curlum ^ *art_colpf1_save;
            new_colour |= new_colour << 16;
            *art_colpf1_save = curlum;
            for (i = 0; i <= 255; i++)
                art_curtable[i] ^= art_curlummask[i] & new_colour;
        }
        if (cl_lookup[C_PF2] ^ *art_colpf2_save) {
            int i;
            ULONG new_colour = cl_lookup[C_PF2] ^ *art_colpf2_save;
            new_colour |= new_colour << 16;
            *art_colpf2_save = cl_lookup[C_PF2];
            for (i = 0; i <= 255; i++)
                art_curtable[i] ^= art_curbkmask[i] & new_colour;
        }
    }
}


/* Initialization ---------------------------------------------------------- */

void ANTIC_Initialise(void) {
    ANTIC_UpdateArtifacting();

    playfield_lookup[0x00] = L_BAK;
    playfield_lookup[0x40] = L_PF0;
    playfield_lookup[0x80] = L_PF1;
    playfield_lookup[0xc0] = L_PF2;
    playfield_lookup[0x100] = L_PF3;
    blank_lookup[0x80] = blank_lookup[0xa0] = blank_lookup[0xc0] = blank_lookup[0xe0] = 0x00;
    hires_mask(0x00) = 0xffff;
    hires_mask(0x40) = HIRES_MASK_01;
    hires_mask(0x80) = HIRES_MASK_10;
    hires_mask(0xc0) = 0xf0f0;
    hires_lum(0x00) = hires_lum(0x40) = hires_lum(0x80) = hires_lum(0xc0) = 0;
    init_pm_lookup();
    mode_e_an_lookup[0] = 0;
    mode_e_an_lookup[1] = mode_e_an_lookup[4] = mode_e_an_lookup[0x10] = mode_e_an_lookup[0x40] = 0;
    mode_e_an_lookup[2] = mode_e_an_lookup[8] = mode_e_an_lookup[0x20] = mode_e_an_lookup[0x80] = 1;
    mode_e_an_lookup[3] = mode_e_an_lookup[12] = mode_e_an_lookup[0x30] = mode_e_an_lookup[0xc0] = 2;
}

void ANTIC_Reset(void) {
    NMIEN = 0x00;
    NMIST = 0x1f;
    ANTIC_PutByte(_DMACTL, 0);
}

/* Border ------------------------------------------------------------------ */

#define DO_BORDER_1 {\
    if (IS_ZERO_ULONG(pm_scanline_ptr)) {\
        ULONG *l_ptr = (ULONG *) ptr;\
        WRITE_VIDEO_LONG(l_ptr++, background); \
        WRITE_VIDEO_LONG(l_ptr++, background); \
        ptr = (UWORD *) l_ptr;\
        pm_scanline_ptr += 4;\
    }\
    else {\
        int k = 4;\
        do

#define DO_BORDER DO_BORDER_1\
            WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[*pm_scanline_ptr++]));\
        while (--k);\
    }\
}

#define DO_GTIA10_BORDER DO_BORDER_1\
            WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[*pm_scanline_ptr++ | 1]));\
        while (--k);\
    }\
}

static void do_border(void)
{
    int kk;
    UWORD *ptr = &scrn_ptr[LBORDER_START];
    const UBYTE *pm_scanline_ptr = &pm_scanline[LBORDER_START];
    ULONG background = lookup_gtia9[0];
    /* left border */
    for (kk = left_border_chars; kk; kk--)
            DO_BORDER
    /* right border */
    ptr = &scrn_ptr[right_border_start];
    pm_scanline_ptr = &pm_scanline[right_border_start];
    while (pm_scanline_ptr < &pm_scanline[RBORDER_END])
            DO_BORDER
}

static void do_border_gtia10(void)
{
    int kk;
    UWORD *ptr = &scrn_ptr[LBORDER_START];
    const UBYTE *pm_scanline_ptr = &pm_scanline[LBORDER_START];
    ULONG background = cl_lookup[C_PM0] | (cl_lookup[C_PM0] << 16);
    /* left border */
    for (kk = left_border_chars; kk; kk--)
        DO_GTIA10_BORDER
    WRITE_VIDEO(ptr, COLOUR(pm_lookup_ptr[*pm_scanline_ptr | 1]));
    /* right border */
    pm_scanline_ptr = &pm_scanline[right_border_start];
    if (pm_scanline_ptr < &pm_scanline[RBORDER_END]) {
        ptr = &scrn_ptr[right_border_start + 1];
        WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[pm_scanline_ptr[1] | 1]));
        WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[pm_scanline_ptr[2] | 1]));
        WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[pm_scanline_ptr[3] | 1]));
        pm_scanline_ptr += 4;
        while (pm_scanline_ptr < &pm_scanline[RBORDER_END])
            DO_GTIA10_BORDER
    }
}

static void do_border_gtia11(void)
{
    int kk;
    UWORD *ptr = &scrn_ptr[LBORDER_START];
    const UBYTE *pm_scanline_ptr = &pm_scanline[LBORDER_START];
    ULONG background = lookup_gtia11[0];
    cl_lookup[C_PF3] &= 0xf0f0;
    cl_lookup[C_BAK] = (UWORD) background;
    /* left border */
    for (kk = left_border_chars; kk; kk--)
        DO_BORDER
    /* right border */
    ptr = &scrn_ptr[right_border_start];
    pm_scanline_ptr = &pm_scanline[right_border_start];
    while (pm_scanline_ptr < &pm_scanline[RBORDER_END])
        DO_BORDER
    COLOUR_TO_WORD(cl_lookup[C_PF3],COLPF3)
    COLOUR_TO_WORD(cl_lookup[C_BAK],COLBK)
}

static void draw_antic_0(void)
{
    UWORD *ptr = scrn_ptr + LBORDER_START;
    if (pm_dirty) {
        const UBYTE *pm_scanline_ptr = &pm_scanline[LBORDER_START];
        ULONG background = lookup_gtia9[0];
        do
            DO_BORDER
        while (pm_scanline_ptr < &pm_scanline[RBORDER_END]);
    }
    else
        FILL_VIDEO(ptr, cl_lookup[C_BAK], (RBORDER_END - LBORDER_START) * 2);
}

static void draw_antic_0_gtia10(void)
{
    UWORD *ptr = scrn_ptr + LBORDER_START;
    if (pm_dirty) {
        const UBYTE *pm_scanline_ptr = &pm_scanline[LBORDER_START];
        ULONG background = cl_lookup[C_PM0] | (cl_lookup[C_PM0] << 16);
        do
            DO_GTIA10_BORDER
        while (pm_scanline_ptr < &pm_scanline[RBORDER_END]);
    }
    else
        FILL_VIDEO(ptr, cl_lookup[C_PM0], (RBORDER_END - LBORDER_START) * 2);
}

static void draw_antic_0_gtia11(void)
{
    UWORD *ptr = scrn_ptr + LBORDER_START;
    if (pm_dirty) {
        const UBYTE *pm_scanline_ptr = &pm_scanline[LBORDER_START];
        ULONG background = lookup_gtia11[0];
        cl_lookup[C_PF3] &= 0xf0f0;
        cl_lookup[C_BAK] = (UWORD) background;
        do
            DO_BORDER
        while (pm_scanline_ptr < &pm_scanline[RBORDER_END]);
        COLOUR_TO_WORD(cl_lookup[C_PF3],COLPF3)
        COLOUR_TO_WORD(cl_lookup[C_BAK],COLBK)
    }
    else
        FILL_VIDEO(ptr, lookup_gtia11[0], (RBORDER_END - LBORDER_START) * 2);
}

/* ANTIC modes ------------------------------------------------------------- */

static UBYTE gtia_10_lookup[] __attribute__((section(".dtcm"))) =
{L_BAK, L_BAK, L_BAK, L_BAK, L_PF0, L_PF1, L_PF2, L_PF3,
 L_BAK, L_BAK, L_BAK, L_BAK, L_PF0, L_PF1, L_PF2, L_PF3};
static UBYTE gtia_10_pm[] __attribute__((section(".dtcm")))=
{1, 2, 4, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static void draw_an_gtia9(const ULONG *t_pm_scanline_ptr)
{
    int i = ((const UBYTE *) t_pm_scanline_ptr - pm_scanline) & ~1;
    while (i < right_border_start) {
        UWORD *ptr = scrn_ptr + i;
        int pixel = (an_scanline[i] << 2) + an_scanline[i + 1];
        UBYTE pm_reg;
        WRITE_VIDEO_LONG((ULONG *) ptr, lookup_gtia9[pixel]);
        pm_reg = pm_scanline[i];
        if (pm_reg) {
            if (pm_reg == L_PF3) {
                WRITE_VIDEO(ptr, pixel | (pixel << 8) | cl_lookup[C_PF3]);
            }
            else {
                WRITE_VIDEO(ptr, COLOUR(pm_reg));
            }
        }
        i++;
        pm_reg = pm_scanline[i];
        if (pm_reg) {
            if (pm_reg == L_PF3) {
                WRITE_VIDEO(ptr + 1, pixel | (pixel << 8) | cl_lookup[C_PF3]);
            }
            else {
                WRITE_VIDEO(ptr + 1, COLOUR(pm_reg));
            }
        }
        i++;
    }
    do_border();
}

static void draw_an_gtia10(const ULONG *t_pm_scanline_ptr)
{
    int i = ((const UBYTE *) t_pm_scanline_ptr - pm_scanline) | 1;
    UWORD lookup_gtia10[16];
    lookup_gtia10[0] = cl_lookup[C_PM0];
    lookup_gtia10[1] = cl_lookup[C_PM1];
    lookup_gtia10[2] = cl_lookup[C_PM2];
    lookup_gtia10[3] = cl_lookup[C_PM3];
    lookup_gtia10[12] = lookup_gtia10[4] = cl_lookup[C_PF0];
    lookup_gtia10[13] = lookup_gtia10[5] = cl_lookup[C_PF1];
    lookup_gtia10[14] = lookup_gtia10[6] = cl_lookup[C_PF2];
    lookup_gtia10[15] = lookup_gtia10[7] = cl_lookup[C_PF3];
    lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = cl_lookup[C_BAK];
    while (i < right_border_start) {
        UWORD *ptr = scrn_ptr + i;
        int pixel = (an_scanline[i - 1] << 2) + an_scanline[i];
        UBYTE pm_reg;
        int colreg;
        pm_reg = pm_scanline[i];
        if (pm_reg) {
            colreg = gtia_10_lookup[pixel];
            PF_COLLS(colreg) |= pm_reg;
            pm_reg |= gtia_10_pm[pixel];
            WRITE_VIDEO(ptr, COLOUR(pm_lookup_ptr[pm_reg] | colreg));
        }
        else {
            WRITE_VIDEO(ptr, lookup_gtia10[pixel]);
        }
        i++;
        pm_reg = pm_scanline[i];
        if (pm_reg) {
            colreg = gtia_10_lookup[pixel];
            PF_COLLS(colreg) |= pm_reg;
            pm_reg |= gtia_10_pm[pixel];
            WRITE_VIDEO(ptr + 1, COLOUR(pm_lookup_ptr[pm_reg] | colreg));
        }
        else {
            WRITE_VIDEO(ptr + 1, lookup_gtia10[pixel]);
        }
        i++;
    }
    do_border_gtia10();
}

static void draw_an_gtia11(const ULONG *t_pm_scanline_ptr)
{
    int i = ((const UBYTE *) t_pm_scanline_ptr - pm_scanline) & ~1;
    while (i < right_border_start) {
        UWORD *ptr = scrn_ptr + i;
        int pixel = (an_scanline[i] << 2) + an_scanline[i + 1];
        UBYTE pm_reg;
        WRITE_VIDEO_LONG((ULONG *) ptr, lookup_gtia11[pixel]);
        pm_reg = pm_scanline[i];
        if (pm_reg) {
            if (pm_reg == L_PF3) {
                WRITE_VIDEO(ptr, pixel ? pixel | (pixel << 8) | cl_lookup[C_PF3] : cl_lookup[C_PF3] & 0xf0f0);
            }
            else {
                WRITE_VIDEO(ptr, COLOUR(pm_reg));
            }
        }
        i++;
        pm_reg = pm_scanline[i];
        if (pm_reg) {
            if (pm_reg == L_PF3) {
                WRITE_VIDEO(ptr + 1, pixel ? pixel | (pixel << 8) | cl_lookup[C_PF3] : cl_lookup[C_PF3] & 0xf0f0);
            }
            else {
                WRITE_VIDEO(ptr + 1, COLOUR(pm_reg));
            }
        }
        i++;
    }
    do_border_gtia11();
}

#define DEFINE_DRAW_AN(anticmode) \
    static void draw_antic_ ## anticmode ## _gtia9 (int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)\
    {\
        prepare_an_antic_ ## anticmode (nchars, ANTIC_memptr, t_pm_scanline_ptr);\
        draw_an_gtia9(t_pm_scanline_ptr);\
    }\
    static void draw_antic_ ## anticmode ## _gtia10 (int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)\
    {\
        prepare_an_antic_ ## anticmode (nchars, ANTIC_memptr, t_pm_scanline_ptr);\
        draw_an_gtia10(t_pm_scanline_ptr);\
    }\
    static void draw_antic_ ## anticmode ## _gtia11 (int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)\
    {\
        prepare_an_antic_ ## anticmode (nchars, ANTIC_memptr, t_pm_scanline_ptr);\
        draw_an_gtia11(t_pm_scanline_ptr);\
    }

#define CHAR_LOOP_BEGIN do {
#define CHAR_LOOP_END } while (--nchars);

#define DO_PMG_LORES PF_COLLS(colreg) |= pm_pixel = *c_pm_scanline_ptr++;\
    WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[pm_pixel] | colreg));

#ifdef ALTERNATE_LOOP_COUNTERS  /* speeds-up pmg in hires a bit or not? try it :) */
#define FOUR_LOOP_BEGIN(data) data |= 0x800000; do {    /* data becomes negative after four data <<= 2 */
#define FOUR_LOOP_END(data) } while (data >= 0);
#else
#define FOUR_LOOP_BEGIN(data) int k = 4; do {
#define FOUR_LOOP_END(data) } while (--k);
#endif

#define INIT_HIRES hires_norm(0x00) = cl_lookup[C_PF2];\
    hires_norm(0x40) = hires_norm(0x10) = hires_norm(0x04) = (cl_lookup[C_PF2] & HIRES_MASK_01) | hires_lum(0x40);\
    hires_norm(0x80) = hires_norm(0x20) = hires_norm(0x08) = (cl_lookup[C_PF2] & HIRES_MASK_10) | hires_lum(0x80);\
    hires_norm(0xc0) = hires_norm(0x30) = hires_norm(0x0c) = (cl_lookup[C_PF2] & 0xf0f0) | hires_lum(0xc0);

#define DO_PMG_HIRES(data) {\
    const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;\
    int pm_pixel;\
    FOUR_LOOP_BEGIN(data)\
        pm_pixel = *c_pm_scanline_ptr++;\
        if (data & 0xc0)\
            PF2PM |= pm_pixel;\
        WRITE_VIDEO(ptr++, (COLOUR(pm_lookup_ptr[pm_pixel] | L_PF2) & hires_mask(data & 0xc0)) | hires_lum(data & 0xc0));\
        data <<= 2;\
    FOUR_LOOP_END(data)\
}

#define DO_PMG_HIRES_NEW(data, tally) {\
    const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;\
    int pm_pixel;\
    FOUR_LOOP_BEGIN(data)\
        pm_pixel = *c_pm_scanline_ptr++;\
        if (pm_pixel) \
            WRITE_VIDEO(ptr++, (COLOUR(pm_lookup_ptr[pm_pixel] | L_PF2)));\
        else\
            WRITE_VIDEO(ptr++, art_lookup_new[(tally & 0xfc0000) >> 18]); \
        data <<= 2;\
        tally <<= 6;\
    FOUR_LOOP_END(data)\
}

#define ADD_FONT_CYCLES xpos += font_cycles[md]

#define INIT_ANTIC_2    const UBYTE *chptr;\
    if (antic_xe_ptr != NULL && chbase_20 < 0x8000 && chbase_20 >= 0x4000)\
        chptr = antic_xe_ptr + ((dctr ^ chbase_20) & 0x3c07);\
    else\
        chptr = AnticMainMemLookup(((dctr ^ chbase_20) & 0xfc07));\
    ADD_FONT_CYCLES;\
    blank_lookup[0x60] = (anticmode == 2 || dctr & 0xe) ? 0xff : 0;\
    blank_lookup[0x00] = blank_lookup[0x20] = blank_lookup[0x40] = (dctr & 0xe) == 8 ? 0 : 0xff;

#define GET_CHDATA_ANTIC_2  chdata = (screendata & invert_mask) ? 0xff : 0;\
    if (blank_lookup[screendata & blank_mask])\
        chdata ^= chptr[(screendata & 0x7f) << 3];

ITCM_CODE static void draw_antic_2(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    INIT_BACKGROUND_6
    INIT_ANTIC_2
    INIT_HIRES

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int chdata;

        GET_CHDATA_ANTIC_2
        if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
            if (chdata) {
                WRITE_VIDEO(ptr++, hires_norm(chdata & 0xc0));
                WRITE_VIDEO(ptr++, hires_norm(chdata & 0x30));
                WRITE_VIDEO(ptr++, hires_norm(chdata & 0x0c));
                WRITE_VIDEO(ptr++, hires_norm((chdata & 0x03) << 2));
            }
            else
                DRAW_BACKGROUND(C_PF2)
        }
        else
            DO_PMG_HIRES(chdata)
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}

static void draw_antic_2_artif(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    ULONG screendata_tally;
    UBYTE screendata = *ANTIC_memptr++;
    UBYTE chdata;
    INIT_ANTIC_2
    GET_CHDATA_ANTIC_2
    screendata_tally = chdata;
    setup_art_colours();

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        ULONG chdata;

        GET_CHDATA_ANTIC_2
        screendata_tally <<= 8;
        screendata_tally |= chdata;
        if (IS_ZERO_ULONG(t_pm_scanline_ptr))
            DRAW_ARTIF
        else {
            chdata = screendata_tally >> 8;
            DO_PMG_HIRES(chdata)
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}


ITCM_CODE static void prepare_an_antic_2(int nchars, const UBYTE *ANTIC_memptr, const ULONG *t_pm_scanline_ptr)
{
    UBYTE *an_ptr = (UBYTE *) t_pm_scanline_ptr + (an_scanline - pm_scanline);
    const UBYTE *chptr;
    if (antic_xe_ptr != NULL && chbase_20 < 0x8000 && chbase_20 >= 0x4000)
        chptr = antic_xe_ptr + ((dctr ^ chbase_20) & 0x3c07);
    else
        chptr = AnticMainMemLookup(((dctr ^ chbase_20) & 0xfc07));

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int chdata;
        GET_CHDATA_ANTIC_2
        *an_ptr++ = chdata >> 6;
        *an_ptr++ = (chdata >> 4) & 3;
        *an_ptr++ = (chdata >> 2) & 3;
        *an_ptr++ = chdata & 3;
    CHAR_LOOP_END
}

ITCM_CODE static void draw_antic_2_gtia9(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    INIT_ANTIC_2
    if ((unsigned long) ptr & 2) { /* HSCROL & 1 */
        prepare_an_antic_2(nchars, ANTIC_memptr, t_pm_scanline_ptr);
        draw_an_gtia9(t_pm_scanline_ptr);
        return;
    }

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int chdata;

        GET_CHDATA_ANTIC_2
        WRITE_VIDEO_LONG((ULONG *) ptr, lookup_gtia9[chdata >> 4]);
        WRITE_VIDEO_LONG((ULONG *) ptr + 1, lookup_gtia9[chdata & 0xf]);
        if (IS_ZERO_ULONG(t_pm_scanline_ptr))
            ptr += 4;
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int k = 4;
            UBYTE pm_reg;
            do {
                pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
                if (pm_reg) {
                    if (pm_reg == L_PF3) {
                        UBYTE tmp = k > 2 ? chdata >> 4 : chdata & 0xf;
                        WRITE_VIDEO(ptr, tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3]);
                    }
                    else
                    {
                        WRITE_VIDEO(ptr, COLOUR(pm_reg));
                    }
                }
                ptr++;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}

static void draw_antic_2_gtia10(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    UWORD lookup_gtia10[16];
    INIT_ANTIC_2
    if ((unsigned long) ptr & 2) { /* HSCROL & 1 */
        prepare_an_antic_2(nchars, ANTIC_memptr, t_pm_scanline_ptr);
        draw_an_gtia10(t_pm_scanline_ptr);
        return;
    }

    lookup_gtia10[0] = cl_lookup[C_PM0];
    lookup_gtia10[1] = cl_lookup[C_PM1];
    lookup_gtia10[2] = cl_lookup[C_PM2];
    lookup_gtia10[3] = cl_lookup[C_PM3];
    lookup_gtia10[12] = lookup_gtia10[4] = cl_lookup[C_PF0];
    lookup_gtia10[13] = lookup_gtia10[5] = cl_lookup[C_PF1];
    lookup_gtia10[14] = lookup_gtia10[6] = cl_lookup[C_PF2];
    lookup_gtia10[15] = lookup_gtia10[7] = cl_lookup[C_PF3];
    lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = cl_lookup[C_BAK];

    ptr++;
    t_pm_scanline_ptr = (const ULONG *) (((const UBYTE *) t_pm_scanline_ptr) + 1);
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int chdata;

        GET_CHDATA_ANTIC_2
        if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
            DO_GTIA_BYTE(ptr, lookup_gtia10, chdata)
            ptr += 4;
        }
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int pm_pixel;
            int colreg;
            int k = 4;
            UBYTE t_screendata = chdata >> 4;
            do {
                colreg = gtia_10_lookup[t_screendata];
                PF_COLLS(colreg) |= pm_pixel = *c_pm_scanline_ptr++;
                pm_pixel |= gtia_10_pm[t_screendata];
                WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[pm_pixel] | colreg));
                if (k == 3)
                    t_screendata = chdata & 0x0f;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border_gtia10();
}

ITCM_CODE static void draw_antic_2_gtia11(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    INIT_ANTIC_2
    if ((unsigned long) ptr & 2) { /* HSCROL & 1 */
        prepare_an_antic_2(nchars, ANTIC_memptr, t_pm_scanline_ptr);
        draw_an_gtia11(t_pm_scanline_ptr);
        return;
    }

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int chdata;

        GET_CHDATA_ANTIC_2
        WRITE_VIDEO_LONG((ULONG *) ptr, lookup_gtia11[chdata >> 4]);
        WRITE_VIDEO_LONG((ULONG *) ptr + 1, lookup_gtia11[chdata & 0xf]);
        if (IS_ZERO_ULONG(t_pm_scanline_ptr))
            ptr += 4;
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int k = 4;
            UBYTE pm_reg;
            do {
                pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
                if (pm_reg) {
                    if (pm_reg == L_PF3) {
                        UBYTE tmp = k > 2 ? chdata & 0xf0 : chdata << 4;
                        WRITE_VIDEO(ptr, tmp ? tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3] : cl_lookup[C_PF3] & 0xf0f0);
                    }
                    else
                    {
                        WRITE_VIDEO(ptr, COLOUR(pm_reg));
                    }
                }
                ptr++;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border_gtia11();
}

ITCM_CODE static void draw_antic_4(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    INIT_BACKGROUND_8
    const UBYTE *chptr;
    if (antic_xe_ptr != NULL && chbase_20 < 0x8000 && chbase_20 >= 0x4000)
        chptr = antic_xe_ptr + (((anticmode == 4 ? dctr : dctr >> 1) ^ chbase_20) & 0x3c07);
    else
        chptr = AnticMainMemLookup((((anticmode == 4 ? dctr : dctr >> 1) ^ chbase_20) & 0xfc07));

    ADD_FONT_CYCLES;
    lookup2[0x0f] = lookup2[0x00] = cl_lookup[C_BAK];
    lookup2[0x4f] = lookup2[0x1f] = lookup2[0x13] =
    lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_lookup[C_PF0];
    lookup2[0x8f] = lookup2[0x2f] = lookup2[0x17] = lookup2[0x11] =
    lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_lookup[C_PF1];
    lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_lookup[C_PF2];
    lookup2[0xcf] = lookup2[0x3f] = lookup2[0x1b] = lookup2[0x12] = cl_lookup[C_PF3];

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        const UWORD *lookup;
        UBYTE chdata;
        if (screendata & 0x80)
            lookup = lookup2 + 0xf;
        else
            lookup = lookup2;
        chdata = chptr[(screendata & 0x7f) << 3];
        if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
            if (chdata) {
                WRITE_VIDEO(ptr++, lookup[chdata & 0xc0]);
                WRITE_VIDEO(ptr++, lookup[chdata & 0x30]);
                WRITE_VIDEO(ptr++, lookup[chdata & 0x0c]);
                WRITE_VIDEO(ptr++, lookup[chdata & 0x03]);
            }
            else
                DRAW_BACKGROUND(C_BAK)
        }
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int pm_pixel;
            int colreg;
            int k = 4;
            playfield_lookup[0xc0] = screendata & 0x80 ? L_PF3 : L_PF2;
            do {
                colreg = playfield_lookup[chdata & 0xc0];
                DO_PMG_LORES
                chdata <<= 2;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    playfield_lookup[0xc0] = L_PF2;
    do_border();
}

static void prepare_an_antic_4(int nchars, const UBYTE *ANTIC_memptr, const ULONG *t_pm_scanline_ptr)
{
    UBYTE *an_ptr = (UBYTE *) t_pm_scanline_ptr + (an_scanline - pm_scanline);
    const UBYTE *chptr;
    if (antic_xe_ptr != NULL && chbase_20 < 0x8000 && chbase_20 >= 0x4000)
        chptr = antic_xe_ptr + (((anticmode == 4 ? dctr : dctr >> 1) ^ chbase_20) & 0x3c07);
    else
        chptr = AnticMainMemLookup((((anticmode == 4 ? dctr : dctr >> 1) ^ chbase_20) & 0xfc07));

    ADD_FONT_CYCLES;
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        UBYTE an;
        UBYTE chdata;
        chdata = chptr[(screendata & 0x3f) << 3];
        an = mode_e_an_lookup[chdata & 0xc0];
        *an_ptr++ = (an == 2 && screendata & 0x80) ? 3 : an;
        an = mode_e_an_lookup[chdata & 0x30];
        *an_ptr++ = (an == 2 && screendata & 0x80) ? 3 : an;
        an = mode_e_an_lookup[chdata & 0x0c];
        *an_ptr++ = (an == 2 && screendata & 0x80) ? 3 : an;
        an = mode_e_an_lookup[chdata & 0x03];
        *an_ptr++ = (an == 2 && screendata & 0x80) ? 3 : an;
    CHAR_LOOP_END
}

DEFINE_DRAW_AN(4)

ITCM_CODE static void draw_antic_6(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    const UBYTE *chptr;
    if (antic_xe_ptr != NULL && chbase_20 < 0x8000 && chbase_20 >= 0x4000)
        chptr = antic_xe_ptr + (((anticmode == 6 ? dctr & 7 : dctr >> 1) ^ chbase_20) - 0x4000);
    else
        chptr = AnticMainMemLookup(((anticmode == 6 ? dctr & 7 : dctr >> 1) ^ chbase_20));

    ADD_FONT_CYCLES;
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        UBYTE chdata;
        UWORD colour;
        int kk = 2;
        colour = COLOUR((playfield_lookup + 0x40)[screendata & 0xc0]);
        chdata = chptr[(screendata & 0x3f) << 3];
        do {
            if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
                if (chdata & 0xf0) {
                    if (chdata & 0x80) {
                        WRITE_VIDEO(ptr++, colour);
                    }
                    else {
                        WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                    }
                    if (chdata & 0x40) {
                        WRITE_VIDEO(ptr++, colour);
                    }
                    else {
                        WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                    }
                    if (chdata & 0x20) {
                        WRITE_VIDEO(ptr++, colour);
                    }
                    else {
                        WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                    }
                    if (chdata & 0x10) {
                        WRITE_VIDEO(ptr++, colour);
                    }
                    else {
                        WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                    }
                }
                else {
                    WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                    WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                    WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                    WRITE_VIDEO(ptr++, cl_lookup[C_BAK]);
                }
                chdata <<= 4;
            }
            else {
                const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
                int pm_pixel;
                UBYTE setcol = (playfield_lookup + 0x40)[screendata & 0xc0];
                int colreg;
                int k = 4;
                do {
                    colreg = chdata & 0x80 ? setcol : L_BAK;
                    DO_PMG_LORES
                    chdata <<= 1;
                } while (--k);

            }
            t_pm_scanline_ptr++;
        } while (--kk);
    CHAR_LOOP_END
    do_border();
}

static void prepare_an_antic_6(int nchars, const UBYTE *ANTIC_memptr, const ULONG *t_pm_scanline_ptr)
{
    UBYTE *an_ptr = (UBYTE *) t_pm_scanline_ptr + (an_scanline - pm_scanline);
    const UBYTE *chptr;
    if (antic_xe_ptr != NULL && chbase_20 < 0x8000 && chbase_20 >= 0x4000)
        chptr = antic_xe_ptr + (((anticmode == 6 ? dctr & 7 : dctr >> 1) ^ chbase_20) - 0x4000);
    else
        chptr = AnticMainMemLookup(((anticmode == 6 ? dctr & 7 : dctr >> 1) ^ chbase_20));

    ADD_FONT_CYCLES;
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        UBYTE an = screendata >> 6;
        UBYTE chdata;
        chdata = chptr[(screendata & 0x3f) << 3];
        *an_ptr++ = chdata & 0x80 ? an : 0;
        *an_ptr++ = chdata & 0x40 ? an : 0;
        *an_ptr++ = chdata & 0x20 ? an : 0;
        *an_ptr++ = chdata & 0x10 ? an : 0;
        *an_ptr++ = chdata & 0x08 ? an : 0;
        *an_ptr++ = chdata & 0x04 ? an : 0;
        *an_ptr++ = chdata & 0x02 ? an : 0;
        *an_ptr++ = chdata & 0x01 ? an : 0;
    CHAR_LOOP_END
}

DEFINE_DRAW_AN(6)

ITCM_CODE static void draw_antic_8(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    lookup2[0x00] = cl_lookup[C_BAK];
    lookup2[0x40] = cl_lookup[C_PF0];
    lookup2[0x80] = cl_lookup[C_PF1];
    lookup2[0xc0] = cl_lookup[C_PF2];
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int kk = 4;
        do {
            if ((const UBYTE *) t_pm_scanline_ptr >= pm_scanline + 4 * (48 - RCHOP))
                break;
            if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
                UWORD data = lookup2[screendata & 0xc0];
                WRITE_VIDEO(ptr++, data);
                WRITE_VIDEO(ptr++, data);
                WRITE_VIDEO(ptr++, data);
                WRITE_VIDEO(ptr++, data);
            }
            else {
                const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
                int pm_pixel;
                int colreg = playfield_lookup[screendata & 0xc0];
                int k = 4;
                do {
                    DO_PMG_LORES
                } while (--k);
            }
            screendata <<= 2;
            t_pm_scanline_ptr++;
        } while (--kk);
    CHAR_LOOP_END
    do_border();
}

static void prepare_an_antic_8(int nchars, const UBYTE *ANTIC_memptr, const ULONG *t_pm_scanline_ptr)
{
    UBYTE *an_ptr = (UBYTE *) t_pm_scanline_ptr + (an_scanline - pm_scanline);
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int kk = 4;
        do {
            UBYTE data = mode_e_an_lookup[screendata & 0xc0];
            *an_ptr++ = data;
            *an_ptr++ = data;
            *an_ptr++ = data;
            *an_ptr++ = data;
            screendata <<= 2;
        } while (--kk);
    CHAR_LOOP_END
}

DEFINE_DRAW_AN(8)

static void draw_antic_9(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    lookup2[0x00] = cl_lookup[C_BAK];
    lookup2[0x80] = lookup2[0x40] = cl_lookup[C_PF0];
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int kk = 4;
        do {
            if ((const UBYTE *) t_pm_scanline_ptr >= pm_scanline + 4 * (48 - RCHOP))
                break;
            if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x80]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x80]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x40]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x40]);
                screendata <<= 2;
            }
            else {
                const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
                int pm_pixel;
                int colreg;
                int k = 4;
                do {
                    colreg = (screendata & 0x80) ? L_PF0 : L_BAK;
                    DO_PMG_LORES
                    if (k & 0x01)
                        screendata <<= 1;
                } while (--k);
            }
            t_pm_scanline_ptr++;
        } while (--kk);
    CHAR_LOOP_END
    do_border();
}

/* ANTIC modes 9, b and c use BAK and PF0 colours only so they're not visible in GTIA modes */

static void draw_antic_9_gtia9(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    draw_antic_0();
}

static void draw_antic_9_gtia10(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    draw_antic_0_gtia10();
}

static void draw_antic_9_gtia11(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    draw_antic_0_gtia11();
}

static void draw_antic_a(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    lookup2[0x00] = cl_lookup[C_BAK];
    lookup2[0x40] = lookup2[0x10] = cl_lookup[C_PF0];
    lookup2[0x80] = lookup2[0x20] = cl_lookup[C_PF1];
    lookup2[0xc0] = lookup2[0x30] = cl_lookup[C_PF2];
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int kk = 2;
        do {
            if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
                WRITE_VIDEO(ptr++, lookup2[screendata & 0xc0]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0xc0]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x30]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x30]);
                screendata <<= 4;
            }
            else {
                const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
                int pm_pixel;
                int colreg;
                int k = 4;
                do {
                    colreg = playfield_lookup[screendata & 0xc0];
                    DO_PMG_LORES
                    if (k & 0x01)
                        screendata <<= 2;
                } while (--k);
            }
            t_pm_scanline_ptr++;
        } while (--kk);
    CHAR_LOOP_END
    do_border();
}

static void prepare_an_antic_a(int nchars, const UBYTE *ANTIC_memptr, const ULONG *t_pm_scanline_ptr)
{
    UBYTE *an_ptr = (UBYTE *) t_pm_scanline_ptr + (an_scanline - pm_scanline);
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        UBYTE data = mode_e_an_lookup[screendata & 0xc0];
        *an_ptr++ = data;
        *an_ptr++ = data;
        data = mode_e_an_lookup[screendata & 0x30];
        *an_ptr++ = data;
        *an_ptr++ = data;
        data = mode_e_an_lookup[screendata & 0x0c];
        *an_ptr++ = data;
        *an_ptr++ = data;
        data = mode_e_an_lookup[screendata & 0x03];
        *an_ptr++ = data;
        *an_ptr++ = data;
    CHAR_LOOP_END
}

DEFINE_DRAW_AN(a)

static void draw_antic_c(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    lookup2[0x00] = cl_lookup[C_BAK];
    lookup2[0x80] = lookup2[0x40] = lookup2[0x20] = lookup2[0x10] = cl_lookup[C_PF0];
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        int kk = 2;
        do {
            if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x80]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x40]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x20]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x10]);
                screendata <<= 4;
            }
            else {
                const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
                int pm_pixel;
                int colreg;
                int k = 4;
                do {
                    colreg = (screendata & 0x80) ? L_PF0 : L_BAK;
                    DO_PMG_LORES
                    screendata <<= 1;
                } while (--k);
            }
            t_pm_scanline_ptr++;
        } while (--kk);
    CHAR_LOOP_END
    do_border();
}

static void draw_antic_e(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    INIT_BACKGROUND_8
    lookup2[0x00] = cl_lookup[C_BAK];
    lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_lookup[C_PF0];
    lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_lookup[C_PF1];
    lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_lookup[C_PF2];

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
            if (screendata) {
                WRITE_VIDEO(ptr++, lookup2[screendata & 0xc0]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x30]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x0c]);
                WRITE_VIDEO(ptr++, lookup2[screendata & 0x03]);
            }
            else
                DRAW_BACKGROUND(C_BAK)
        }
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int pm_pixel;
            int colreg;
            int k = 4;
            do {
                colreg = playfield_lookup[screendata & 0xc0];
                DO_PMG_LORES
                screendata <<= 2;
            } while (--k);

        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}

static void prepare_an_antic_e(int nchars, const UBYTE *ANTIC_memptr, const ULONG *t_pm_scanline_ptr)
{
    UBYTE *an_ptr = (UBYTE *) t_pm_scanline_ptr + (an_scanline - pm_scanline);
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        *an_ptr++ = mode_e_an_lookup[screendata & 0xc0];
        *an_ptr++ = mode_e_an_lookup[screendata & 0x30];
        *an_ptr++ = mode_e_an_lookup[screendata & 0x0c];
        *an_ptr++ = mode_e_an_lookup[screendata & 0x03];
    CHAR_LOOP_END
}

static void draw_antic_e_gtia9(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    ULONG lookup[16];
    if ((unsigned long) ptr & 2) { /* HSCROL & 1 */
        prepare_an_antic_e(nchars, ANTIC_memptr, t_pm_scanline_ptr);
        draw_an_gtia9(t_pm_scanline_ptr);
        return;
    }
    lookup[0] = lookup[1] = lookup[4] = lookup[5] = lookup_gtia9[0];
    lookup[2] = lookup[6] = lookup_gtia9[1];
    lookup[3] = lookup[7] = lookup_gtia9[2];
    lookup[8] = lookup[9] = lookup_gtia9[4];
    lookup[10] = lookup_gtia9[5];
    lookup[11] = lookup_gtia9[6];
    lookup[12] = lookup[13] = lookup_gtia9[8];
    lookup[14] = lookup_gtia9[9];
    lookup[15] = lookup_gtia9[10];
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        WRITE_VIDEO_LONG((ULONG *) ptr, lookup[screendata >> 4]);
        WRITE_VIDEO_LONG((ULONG *) ptr + 1, lookup[screendata & 0xf]);
        if (IS_ZERO_ULONG(t_pm_scanline_ptr))
            ptr += 4;
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int k = 4;
            UBYTE pm_reg;
            do {
                pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
                if (pm_reg) {
                    if (pm_reg == L_PF3) {
                        UBYTE tmp = k > 2 ? screendata >> 4 : screendata & 0xf;
                        WRITE_VIDEO(ptr, tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3]);
                    }
                    else
                    {
                        WRITE_VIDEO(ptr, COLOUR(pm_reg));
                    }
                }
                ptr++;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}

static void draw_antic_e_gtia10 (int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    prepare_an_antic_e(nchars, ANTIC_memptr, t_pm_scanline_ptr);
    draw_an_gtia10(t_pm_scanline_ptr);
}
static void draw_antic_e_gtia11 (int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    prepare_an_antic_e(nchars, ANTIC_memptr, t_pm_scanline_ptr);
    draw_an_gtia11(t_pm_scanline_ptr);
}

static void draw_antic_f(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    INIT_BACKGROUND_6
    INIT_HIRES

    CHAR_LOOP_BEGIN
        int screendata = *ANTIC_memptr++;
        if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
            if (screendata) {
                WRITE_VIDEO(ptr++, hires_norm(screendata & 0xc0));
                WRITE_VIDEO(ptr++, hires_norm(screendata & 0x30));
                WRITE_VIDEO(ptr++, hires_norm(screendata & 0x0c));
                WRITE_VIDEO(ptr++, hires_norm((screendata & 0x03) << 2));
            }
            else
                DRAW_BACKGROUND(C_PF2)
        }
        else
            DO_PMG_HIRES(screendata)
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}

static void draw_antic_f_artif(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    ULONG screendata_tally = *ANTIC_memptr++;

    setup_art_colours();
    CHAR_LOOP_BEGIN
        int screendata = *ANTIC_memptr++;
        screendata_tally <<= 8;
        screendata_tally |= screendata;
        if (IS_ZERO_ULONG(t_pm_scanline_ptr))
            DRAW_ARTIF
        else {
            screendata = ANTIC_memptr[-2];
            DO_PMG_HIRES(screendata)
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}


static void prepare_an_antic_f(int nchars, const UBYTE *ANTIC_memptr, const ULONG *t_pm_scanline_ptr)
{
    UBYTE *an_ptr = (UBYTE *) t_pm_scanline_ptr + (an_scanline - pm_scanline);
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        *an_ptr++ = screendata >> 6;
        *an_ptr++ = (screendata >> 4) & 3;
        *an_ptr++ = (screendata >> 2) & 3;
        *an_ptr++ = screendata & 3;
    CHAR_LOOP_END
}

static void draw_antic_f_gtia9(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    if ((unsigned long) ptr & 2) { /* HSCROL & 1 */
        prepare_an_antic_f(nchars, ANTIC_memptr, t_pm_scanline_ptr);
        draw_an_gtia9(t_pm_scanline_ptr);
        return;
    }
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        WRITE_VIDEO_LONG((ULONG *) ptr, lookup_gtia9[screendata >> 4]);
        WRITE_VIDEO_LONG((ULONG *) ptr + 1, lookup_gtia9[screendata & 0xf]);
        if (IS_ZERO_ULONG(t_pm_scanline_ptr))
            ptr += 4;
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int k = 4;
            UBYTE pm_reg;
            do {
                pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
                if (pm_reg) {
                    if (pm_reg == L_PF3) {
                        UBYTE tmp = k > 2 ? screendata >> 4 : screendata & 0xf;
                        WRITE_VIDEO(ptr, tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3]);
                    }
                    else {
                        WRITE_VIDEO(ptr, COLOUR(pm_reg));
                    }
                }
                ptr++;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}

static void draw_antic_f_gtia10(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    UWORD lookup_gtia10[16];

    if ((unsigned long) ptr & 2) { /* HSCROL & 1 */
        prepare_an_antic_f(nchars, ANTIC_memptr, t_pm_scanline_ptr);
        draw_an_gtia10(t_pm_scanline_ptr);
        return;
    }
    
    lookup_gtia10[0] = cl_lookup[C_PM0];
    lookup_gtia10[1] = cl_lookup[C_PM1];
    lookup_gtia10[2] = cl_lookup[C_PM2];
    lookup_gtia10[3] = cl_lookup[C_PM3];
    lookup_gtia10[12] = lookup_gtia10[4] = cl_lookup[C_PF0];
    lookup_gtia10[13] = lookup_gtia10[5] = cl_lookup[C_PF1];
    lookup_gtia10[14] = lookup_gtia10[6] = cl_lookup[C_PF2];
    lookup_gtia10[15] = lookup_gtia10[7] = cl_lookup[C_PF3];
    lookup_gtia10[8] = lookup_gtia10[9] = lookup_gtia10[10] = lookup_gtia10[11] = cl_lookup[C_BAK];

    ptr++;
    t_pm_scanline_ptr = (const ULONG *) (((const UBYTE *) t_pm_scanline_ptr) + 1);
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
            DO_GTIA_BYTE(ptr, lookup_gtia10, screendata)
            ptr += 4;
        }
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int pm_pixel;
            int colreg;
            int k = 4;
            UBYTE t_screendata = screendata >> 4;
            do {
                colreg = gtia_10_lookup[t_screendata];
                PF_COLLS(colreg) |= pm_pixel = *c_pm_scanline_ptr++;
                pm_pixel |= gtia_10_pm[t_screendata];
                WRITE_VIDEO(ptr++, COLOUR(pm_lookup_ptr[pm_pixel] | colreg));
                if (k == 3)
                    t_screendata = screendata & 0x0f;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border_gtia10();
}

static void draw_antic_f_gtia11(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    if ((unsigned long) ptr & 2) { /* HSCROL & 1 */
        prepare_an_antic_f(nchars, ANTIC_memptr, t_pm_scanline_ptr);
        draw_an_gtia11(t_pm_scanline_ptr);
        return;
    }
    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        WRITE_VIDEO_LONG((ULONG *) ptr, lookup_gtia11[screendata >> 4]);
        WRITE_VIDEO_LONG((ULONG *) ptr + 1, lookup_gtia11[screendata & 0xf]);
        if (IS_ZERO_ULONG(t_pm_scanline_ptr))
            ptr += 4;
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int k = 4;
            UBYTE pm_reg;
            do {
                pm_reg = pm_lookup_ptr[*c_pm_scanline_ptr++];
                if (pm_reg) {
                    if (pm_reg == L_PF3) {
                        UBYTE tmp = k > 2 ? screendata & 0xf0 : screendata << 4;
                        WRITE_VIDEO(ptr, tmp ? tmp | ((UWORD)tmp << 8) | cl_lookup[C_PF3] : cl_lookup[C_PF3] & 0xf0f0);
                    }
                    else
                    {
                        WRITE_VIDEO(ptr, COLOUR(pm_reg));
                    }
                }
                ptr++;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border_gtia11();
}

/* GTIA-switch-to-mode-00 bug
If while drawing line in hi-res mode PRIOR is changed from 0x40..0xff to
0x00..0x3f, GTIA doesn't back to hi-res, but starts generating mode similar
to ANTIC's 0xe, but with colours PF0, PF1, PF2, PF3. */

static void draw_antic_f_gtia_bug(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr)
{
    lookup2[0x00] = cl_lookup[C_PF0];
    lookup2[0x40] = lookup2[0x10] = lookup2[0x04] = lookup2[0x01] = cl_lookup[C_PF1];
    lookup2[0x80] = lookup2[0x20] = lookup2[0x08] = lookup2[0x02] = cl_lookup[C_PF2];
    lookup2[0xc0] = lookup2[0x30] = lookup2[0x0c] = lookup2[0x03] = cl_lookup[C_PF3];

    CHAR_LOOP_BEGIN
        UBYTE screendata = *ANTIC_memptr++;
        if (IS_ZERO_ULONG(t_pm_scanline_ptr)) {
            WRITE_VIDEO(ptr++, lookup2[screendata & 0xc0]);
            WRITE_VIDEO(ptr++, lookup2[screendata & 0x30]);
            WRITE_VIDEO(ptr++, lookup2[screendata & 0x0c]);
            WRITE_VIDEO(ptr++, lookup2[screendata & 0x03]);
        }
        else {
            const UBYTE *c_pm_scanline_ptr = (const UBYTE *) t_pm_scanline_ptr;
            int pm_pixel;
            int colreg;
            int k = 4;
            do {
                colreg = (playfield_lookup + 0x40)[screendata & 0xc0];
                DO_PMG_LORES
                screendata <<= 2;
            } while (--k);
        }
        t_pm_scanline_ptr++;
    CHAR_LOOP_END
    do_border();
}

/* pointer to a function that draws a single line of graphics */
typedef void (*draw_antic_function)(int nchars, const UBYTE *ANTIC_memptr, UWORD *ptr, const ULONG *t_pm_scanline_ptr);

/* tables for all GTIA and ANTIC modes */
static draw_antic_function draw_antic_table[4][16] = {
/* normal */
        { NULL,         NULL,           draw_antic_2,   draw_antic_2,
        draw_antic_4,   draw_antic_4,   draw_antic_6,   draw_antic_6,
        draw_antic_8,   draw_antic_9,   draw_antic_a,   draw_antic_c,
        draw_antic_c,   draw_antic_e,   draw_antic_e,   draw_antic_f},
/* GTIA 9 */
        { NULL,         NULL,           draw_antic_2_gtia9, draw_antic_2_gtia9,
        draw_antic_4_gtia9, draw_antic_4_gtia9, draw_antic_6_gtia9, draw_antic_6_gtia9,
        draw_antic_8_gtia9, draw_antic_9_gtia9, draw_antic_a_gtia9, draw_antic_9_gtia9,
        draw_antic_9_gtia9, draw_antic_e_gtia9, draw_antic_e_gtia9, draw_antic_f_gtia9},
/* GTIA 10 */
        { NULL,         NULL,           draw_antic_2_gtia10,    draw_antic_2_gtia10,
        draw_antic_4_gtia10,    draw_antic_4_gtia10,    draw_antic_6_gtia10,    draw_antic_6_gtia10,
        draw_antic_8_gtia10,    draw_antic_9_gtia10,    draw_antic_a_gtia10,    draw_antic_9_gtia10,
        draw_antic_9_gtia10,    draw_antic_e_gtia10,    draw_antic_e_gtia10,    draw_antic_f_gtia10},
/* GTIA 11 */
        { NULL,         NULL,           draw_antic_2_gtia11,    draw_antic_2_gtia11,
        draw_antic_4_gtia11,    draw_antic_4_gtia11,    draw_antic_6_gtia11,    draw_antic_6_gtia11,
        draw_antic_8_gtia11,    draw_antic_9_gtia11,    draw_antic_a_gtia11,    draw_antic_9_gtia11,
        draw_antic_9_gtia11,    draw_antic_e_gtia11,    draw_antic_e_gtia11,    draw_antic_f_gtia11}};

/* pointer to current GTIA/ANTIC mode routine */
draw_antic_function draw_antic_ptr = draw_antic_8;
/* pointer to current GTIA mode blank drawing routine */
void (*draw_antic_0_ptr)(void) = draw_antic_0;

/* Artifacting ------------------------------------------------------------ */

void ANTIC_UpdateArtifacting(void)
{
#define ART_BROWN 0
#define ART_BLUE 1
#define ART_DARK_BROWN 2
#define ART_DARK_BLUE 3
#define ART_BRIGHT_BROWN 4
#define ART_BRIGHT_BLUE 5
#define ART_RED 6
#define ART_GREEN 7
    static const UBYTE art_colour_table[4][8] = {
        { 0x88, 0x14, 0x88, 0x14, 0x8f, 0x1f, 0xbb, 0x5f }, /* brownblue */
        { 0x14, 0x88, 0x14, 0x88, 0x1f, 0x8f, 0x5f, 0xbb }, /* bluebrown */
        { 0xd6, 0x46, 0xd6, 0x46, 0xdf, 0x4a, 0x4f, 0xac }, /* redgreen */
        { 0x46, 0xd6, 0x46, 0xd6, 0x4a, 0xdf, 0xac, 0x4f }  /* greenred */
    };

    int i;
    int j;
    int c;
    const UBYTE *art_colours;
    UBYTE q;
    UBYTE art_white;

    if (myConfig.artifacting == 0) {
        draw_antic_table[0][2] = draw_antic_table[0][3] = draw_antic_2;
        draw_antic_table[0][0xf] = draw_antic_f;
        return;
    }

  draw_antic_table[0][2] = draw_antic_table[0][3] = draw_antic_2_artif;
  draw_antic_table[0][0xf] = draw_antic_f_artif;

    art_colours = (myConfig.artifacting <= 4 ? art_colour_table[myConfig.artifacting - 1] : art_colour_table[2]);

    art_reverse_colpf1_save = art_normal_colpf1_save = cl_lookup[C_PF1] & 0x0f0f;
    art_reverse_colpf2_save = art_normal_colpf2_save = cl_lookup[C_PF2];
    art_white = (cl_lookup[C_PF2] & 0xf0) | (cl_lookup[C_PF1] & 0x0f);

    for (i = 0; i <= 255; i++) {
        art_bkmask_normal[i] = 0;
        art_lummask_normal[i] = 0;
        art_bkmask_reverse[255 - i] = 0;
        art_lummask_reverse[255 - i] = 0;

        for (j = 0; j <= 3; j++) {
            q = i << j;
            if (!(q & 0x20)) {
                if ((q & 0xf8) == 0x50)
                    c = ART_BLUE;               /* 01010 */
                else if ((q & 0xf8) == 0xD8)
                    c = ART_DARK_BLUE;          /* 11011 */
                else {                          /* xx0xx */
                    ((UBYTE *) art_lookup_normal)[(i << 2) + j] = COLPF2;
                    ((UBYTE *) art_lookup_reverse)[((255 - i) << 2) + j] = art_white;
                    ((UBYTE *) art_bkmask_normal)[(i << 2) + j] = 0xff;
                    ((UBYTE *) art_lummask_reverse)[((255 - i) << 2) + j] = 0x0f;
                    ((UBYTE *) art_bkmask_reverse)[((255 - i) << 2) + j] = 0xf0;
                    continue;
                }
            }
            else if (q & 0x40) {
                if (q & 0x10)
                    goto colpf1_pixel;          /* x111x */
                else if (q & 0x80) {
                    if (q & 0x08)
                        c = ART_BRIGHT_BROWN;   /* 11101 */
                    else
                        goto colpf1_pixel;      /* 11100 */
                }
                else
                    c = ART_GREEN;              /* 0110x */
            }
            else if (q & 0x10) {
                if (q & 0x08) {
                    if (q & 0x80)
                        c = ART_BRIGHT_BROWN;   /* 00111 */
                    else
                        goto colpf1_pixel;      /* 10111 */
                }
                else
                    c = ART_RED;                /* x0110 */
            }
            else
                c = ART_BROWN;                  /* x010x */

            ((UBYTE *) art_lookup_reverse)[((255 - i) << 2) + j] =
            ((UBYTE *) art_lookup_normal)[(i << 2) + j] = art_colours[(j & 1) ^ c];
            continue;

            colpf1_pixel:
            ((UBYTE *) art_lookup_normal)[(i << 2) + j] = art_white;
            ((UBYTE *) art_lookup_reverse)[((255 - i) << 2) + j] = COLPF2;
            ((UBYTE *) art_bkmask_reverse)[((255 - i) << 2) + j] = 0xff;
            ((UBYTE *) art_lummask_normal)[(i << 2) + j] = 0x0f;
            ((UBYTE *) art_bkmask_normal)[(i << 2) + j] = 0xf0;
        }
    }
}


/* Display List ------------------------------------------------------------ */

inline UBYTE ANTIC_GetDLByte(UWORD *paddr)
{
    int addr = *paddr;
    UBYTE result;
    result = dGetByte(addr);
    addr++;
    if ((addr & 0x3FF) == 0)
        addr -= 0x400;
    *paddr = (UWORD) addr;
    return result;
}

inline UWORD ANTIC_GetDLWord(UWORD *paddr)
{
    UBYTE lsb = ANTIC_GetDLByte(paddr);
    if (player_flickering && ((VDELAY & 0x80) == 0 || ypos & 1))
        GRAFP3 = lsb;
    return (ANTIC_GetDLByte(paddr) << 8) + lsb;
}


/* Real ANTIC doesn't fetch beginning bytes in HSC
   nor screen+47 in wide playfield. This function does. */
static void ANTIC_load(void)
{
    UWORD new_screenaddr = screenaddr + chars_read[md];
    if ((screenaddr ^ new_screenaddr) & 0xf000) {
        int bytes = (-screenaddr) & 0xfff;
        if (antic_xe_ptr != NULL && screenaddr < 0x8000 && screenaddr >= 0x4000) {
            memcpy(ANTIC_memory + ANTIC_margin, antic_xe_ptr + (screenaddr - 0x4000), bytes);
            if (new_screenaddr & 0xfff)
                memcpy(ANTIC_memory + ANTIC_margin + bytes, antic_xe_ptr + (screenaddr + bytes - 0x5000), new_screenaddr & 0xfff);
        }
        else if ((screenaddr & 0xf000) == 0xd000) {
            CopyFromMem(screenaddr, ANTIC_memory + ANTIC_margin, bytes);
            if (new_screenaddr & 0xfff)
                CopyFromMem((UWORD) (screenaddr + bytes - 0x1000), ANTIC_memory + ANTIC_margin + bytes, new_screenaddr & 0xfff);
        }
        else {
            dCopyFromMem(screenaddr, ANTIC_memory + ANTIC_margin, bytes);
            if (new_screenaddr & 0xfff)
                dCopyFromMem(screenaddr + bytes - 0x1000, ANTIC_memory + ANTIC_margin + bytes, new_screenaddr & 0xfff);
        }
        screenaddr = new_screenaddr - 0x1000;
    }
    else {
        if (antic_xe_ptr != NULL && screenaddr < 0x8000 && screenaddr >= 0x4000)
            memcpy(ANTIC_memory + ANTIC_margin, antic_xe_ptr + (screenaddr - 0x4000), chars_read[md]);
        else if ((screenaddr & 0xf000) == 0xd000)
            CopyFromMem(screenaddr, ANTIC_memory + ANTIC_margin, chars_read[md]);
        else
            dCopyFromMem(screenaddr, ANTIC_memory + ANTIC_margin, chars_read[md]);
        screenaddr = new_screenaddr;
    }
}

UBYTE mode_type[32] __attribute__((section(".dtcm"))) = {
        NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL0, NORMAL1, NORMAL1,
        NORMAL2, NORMAL2, NORMAL1, NORMAL1, NORMAL1, NORMAL0, NORMAL0, NORMAL0,
        SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL0, SCROLL1, SCROLL1,
        SCROLL2, SCROLL2, SCROLL1, SCROLL1, SCROLL1, SCROLL0, SCROLL0, SCROLL0
    };
UBYTE normal_lastline[16] __attribute__((section(".dtcm"))) = { 0, 0, 7, 9, 7, 15, 7, 15, 7, 3, 3, 1, 0, 1, 0, 0 };

__attribute__((noinline)) void ANTIC_FrameSetup()
{
    ypos = 0;
    do {
        POKEY_Scanline();       /* check and generate IRQ */
        OVERSCREEN_LINE;
    } while (ypos < 8);

    // Direct screen write...
    if (myConfig.alphaBlend)
    {
        scrn_ptr = bgGetGfxPtr((gTotalAtariFrames&1) ? bg2:bg3);
    }
    else
    {
        scrn_ptr = bgGetGfxPtr(bg2);
    }

    need_dl = TRUE;
}

/* This function emulates one frame drawing screen at atari_screen */
ITCM_CODE void ANTIC_Frame(int draw_display) 
{
    UBYTE vscrol_flag = FALSE;
    UBYTE no_jvb = TRUE;
    UBYTE need_load;

    ANTIC_FrameSetup();
    
    do {
        POKEY_Scanline();       /* check and generate IRQ */
        pmg_dma();

        need_load = FALSE;
        if (need_dl) {
            if (DMACTL & 0x20) {
                IR = ANTIC_GetDLByte(&dlist);
                anticmode = IR & 0xf;
                xpos++;
                /* PMG flickering :-) */
                if (missile_flickering)
                    GRAFM = ypos & 1 ? IR : ((GRAFM ^ IR) & hold_missiles_tab[VDELAY & 0xf]) ^ IR;
                if (player_flickering) {
                    UBYTE hold = ypos & 1 ? 0 : VDELAY;
                    if ((hold & 0x10) == 0)
                        GRAFP0 = dGetByte((UWORD) (regPC - xpos + 8));
                    if ((hold & 0x20) == 0)
                        GRAFP1 = dGetByte((UWORD) (regPC - xpos + 9));
                    if ((hold & 0x40) == 0)
                        GRAFP2 = dGetByte((UWORD) (regPC - xpos + 10));
                    if ((hold & 0x80) == 0)
                        GRAFP3 = dGetByte((UWORD) (regPC - xpos + 11));
                }
            }
            else
                IR &= 0x7f; /* repeat last instruction, but don't generate DLI */

            dctr = 0;
            need_dl = FALSE;
            vscrol_off = FALSE;

            switch (anticmode) {
            case 0x00:
                lastline = (IR >> 4) & 7;
                if (vscrol_flag) {
                    lastline = VSCROL;
                    vscrol_flag = FALSE;
                    vscrol_off = TRUE;
                }
                break;
            case 0x01:
                lastline = 0;
                if (IR & 0x40 && DMACTL & 0x20) {
                    dlist = ANTIC_GetDLWord(&dlist);
                    xpos += 2;
                    no_jvb = FALSE;
                }
                else
                    if (vscrol_flag) {
                        lastline = VSCROL;
                        vscrol_flag = FALSE;
                        vscrol_off = TRUE;
                    }
                break;
            default:
                lastline = normal_lastline[anticmode];
                if (IR & 0x20) {
                    if (!vscrol_flag) {
                        GO(VSCON_C);
                        dctr = VSCROL;
                        vscrol_flag = TRUE;
                    }
                }
                else if (vscrol_flag) {
                    lastline = VSCROL;
                    vscrol_flag = FALSE;
                    vscrol_off = TRUE;
                }
                if (IR & 0x40 && DMACTL & 0x20) {
                    screenaddr = ANTIC_GetDLWord(&dlist);
                    xpos += 2;
                }
                md = mode_type[IR & 0x1f];
                need_load = TRUE;
                draw_antic_ptr = draw_antic_table[PRIOR >> 6][anticmode];
                break;
            }
        }

        if ((IR & 0x4f) == 1 && (DMACTL & 0x20)) {
            dlist = ANTIC_GetDLWord(&dlist);
            xpos += 2;
        }

        if (dctr == lastline) {
            if (no_jvb)
                need_dl = TRUE;
            if (IR & 0x80) {
                GO(NMIST_C);
                NMIST = 0x9f;
                if (NMIEN & 0x80) {
                    GO(NMI_C);
                    NMI();
                }
            }
        }

        if (need_load && anticmode <= 5 && DMACTL & 3)
            xpos += before_cycles[md];

        GO(SCR_C);
        new_pm_scanline();

        xpos += DMAR;
        
        if (anticmode < 2 || (DMACTL & 3) == 0) 
        {
            if (draw_display) draw_antic_0_ptr();
            GOEOL;
            scrn_ptr += 256;
            if (no_jvb) {
                dctr++;
                dctr &= 0xf;
            }
            continue;
        }

        if (need_load) {
            if (draw_display) ANTIC_load();
            xpos += load_cycles[md];
            if (anticmode <= 5) /* extra cycles in font modes */
                xpos -= extra_cycles[md];
        }

        if (draw_display) draw_antic_ptr(chars_displayed[md], ANTIC_memory + ANTIC_margin + ch_offset[md], scrn_ptr + x_min[md], (ULONG *) &pm_scanline[x_min[md]]);

        GOEOL;
        scrn_ptr += 256;
        dctr++;
        dctr &= 0xf;
    } while (ypos < (ATARI_HEIGHT + 8));
    
    POKEY_Scanline();       /* check and generate IRQ */
    GO(NMIST_C);
    NMIST = 0x5f;               /* Set VBLANK */
    if (NMIEN & 0x40) {
        GO(NMI_C);
        NMI();
    }
    xpos += DMAR;
    GOEOL;

    do {
        POKEY_Scanline();       /* check and generate IRQ */
        OVERSCREEN_LINE;
    } while (ypos < max_ypos);
    ypos = 0; /* just for monitor.c */
}

/* ANTIC registers --------------------------------------------------------- */

UBYTE ANTIC_GetByte(UWORD addr)
{
    switch (addr & 0xf) {
    case _VCOUNT:
        if (XPOS < LINE_C)
            return ypos >> 1;
        if (ypos + 1 < max_ypos)
            return (ypos + 1) >> 1;
        return 0;
    case _PENH:
        return PENH;
    case _PENV:
        return PENV;
    case _NMIST:
        return NMIST;
    default:
        return 0xff;
    }
}


/* GTIA calls it on write to PRIOR */
void set_prior(UBYTE byte)
{
    if ((byte ^ PRIOR) & 0x0f) {
        UWORD cword = 0;
        UWORD cword2 = 0;
        if ((byte & 3) == 0) {
            cword = cl_lookup[C_PF0];
            cword2 = cl_lookup[C_PF1];
        }
        if ((byte & 0xc) == 0) {
            cl_lookup[C_PF0 | C_PM0] = cword | cl_lookup[C_PM0];
            cl_lookup[C_PF0 | C_PM1] = cword | cl_lookup[C_PM1];
            cl_lookup[C_PF0 | C_PM01] = cword | cl_lookup[C_PM01];
            cl_lookup[C_PF1 | C_PM0] = cword2 | cl_lookup[C_PM0];
            cl_lookup[C_PF1 | C_PM1] = cword2 | cl_lookup[C_PM1];
            cl_lookup[C_PF1 | C_PM01] = cword2 | cl_lookup[C_PM01];
        }
        else {
            cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF0 | C_PM0] = cword;
            cl_lookup[C_PF1 | C_PM01] = cl_lookup[C_PF1 | C_PM1] = cl_lookup[C_PF1 | C_PM0] = cword2;
        }
        if (byte & 4) {
            cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PF2];
            cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF3 | C_PM0] = cl_lookup[C_PF3];
        }
        else {
            cl_lookup[C_PF3 | C_PM0] = cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PM0];
            cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PM1];
            cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PM01];
        }
        cword = cword2 = 0;
        if ((byte & 9) == 0) {
            cword = cl_lookup[C_PF2];
            cword2 = cl_lookup[C_PF3];
        }
        if ((byte & 6) == 0) {
            cl_lookup[C_PF2 | C_PM2] = cword | cl_lookup[C_PM2];
            cl_lookup[C_PF2 | C_PM3] = cword | cl_lookup[C_PM3];
            cl_lookup[C_PF2 | C_PM23] = cword | cl_lookup[C_PM23];
            cl_lookup[C_PF3 | C_PM2] = cword2 | cl_lookup[C_PM2];
            cl_lookup[C_PF3 | C_PM3] = cword2 | cl_lookup[C_PM3];
            cl_lookup[C_PF3 | C_PM23] = cword2 | cl_lookup[C_PM23];
        }
        else {
            cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF2 | C_PM2] = cword;
            cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM3] = cl_lookup[C_PF3 | C_PM2] = cword2;
        }

        if (byte & 1) {
            cl_lookup[C_PF1 | C_PM2] = cl_lookup[C_PF0 | C_PM2] = cl_lookup[C_PM2];
            cl_lookup[C_PF1 | C_PM3] = cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PM3];
            cl_lookup[C_PF1 | C_PM23] = cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PM23];
        }
        else {
            cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF0 | C_PM2] = cl_lookup[C_PF0];
            cl_lookup[C_PF1 | C_PM23] = cl_lookup[C_PF1 | C_PM3] = cl_lookup[C_PF1 | C_PM2] = cl_lookup[C_PF1];
        }
        if ((byte & 0xf) == 0xc) {
            cl_lookup[C_PF0 | C_PM0123] = cl_lookup[C_PF0 | C_PM123] = cl_lookup[C_PF0 | C_PM023] = cl_lookup[C_PF0];
            cl_lookup[C_PF1 | C_PM0123] = cl_lookup[C_PF1 | C_PM123] = cl_lookup[C_PF1 | C_PM023] = cl_lookup[C_PF1];
        }
        else
            cl_lookup[C_PF0 | C_PM0123] = cl_lookup[C_PF0 | C_PM123] = cl_lookup[C_PF0 | C_PM023] =
            cl_lookup[C_PF1 | C_PM0123] = cl_lookup[C_PF1 | C_PM123] = cl_lookup[C_PF1 | C_PM023] = COLOUR_BLACK;
        if (byte & 0xf) {
            cl_lookup[C_PF0 | C_PM25] = cl_lookup[C_PF0];
            cl_lookup[C_PF1 | C_PM25] = cl_lookup[C_PF1];
            cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = COLOUR_BLACK;
        }
        else {
            cl_lookup[C_PF0 | C_PM235] = cl_lookup[C_PF0 | C_PM35] = cl_lookup[C_PF0 | C_PM25] =
            cl_lookup[C_PF1 | C_PM235] = cl_lookup[C_PF1 | C_PM35] = cl_lookup[C_PF1 | C_PM25] = cl_lookup[C_PF3];
            cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2];
            cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3];
            cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23];
        }
    }
    pm_lookup_ptr = pm_lookup_table[prior_to_pm_lookup[byte & 0x3f]];
    draw_antic_0_ptr = byte < 0x80 ? draw_antic_0 : byte < 0xc0 ? draw_antic_0_gtia10 : draw_antic_0_gtia11;
    if (byte < 0x40 && (PRIOR >= 0x40 || draw_antic_ptr == draw_antic_f_gtia_bug) && anticmode == 0xf && XPOS >= ((DMACTL & 3) == 3 ? 16 : 18))
        draw_antic_ptr = draw_antic_f_gtia_bug;
    else
        draw_antic_ptr = draw_antic_table[byte >> 6][anticmode];
}


void ANTIC_PutByte(UWORD addr, UBYTE byte)
{
    switch (addr & 0xf) {
    case _DLISTL:
        dlist = (dlist & 0xff00) | byte;
        break;
    case _DLISTH:
        dlist = (dlist & 0x00ff) | (byte << 8);
        break;
    case _DMACTL:
        DMACTL = byte;
        switch (byte & 0x03) {
        case 0x00:
            /* no ANTIC_load when screen off */
            /* chars_read[NORMAL0] = 0;
            chars_read[NORMAL1] = 0;
            chars_read[NORMAL2] = 0;
            chars_read[SCROLL0] = 0;
            chars_read[SCROLL1] = 0;
            chars_read[SCROLL2] = 0; */
            /* no draw_antic_* when screen off */
            /* chars_displayed[NORMAL0] = 0;
            chars_displayed[NORMAL1] = 0;
            chars_displayed[NORMAL2] = 0;
            chars_displayed[SCROLL0] = 0;
            chars_displayed[SCROLL1] = 0;
            chars_displayed[SCROLL2] = 0;
            x_min[NORMAL0] = 0;
            x_min[NORMAL1] = 0;
            x_min[NORMAL2] = 0;
            x_min[SCROLL0] = 0;
            x_min[SCROLL1] = 0;
            x_min[SCROLL2] = 0;
            ch_offset[NORMAL0] = 0;
            ch_offset[NORMAL1] = 0;
            ch_offset[NORMAL2] = 0;
            ch_offset[SCROLL0] = 0;
            ch_offset[SCROLL1] = 0;
            ch_offset[SCROLL2] = 0; */
            /* no borders when screen off, only background */
            /* left_border_chars = 48 - LCHOP - RCHOP;
            right_border_start = 0; */
            break;
        case 0x01:
            chars_read[NORMAL0] = 32;
            chars_read[NORMAL1] = 16;
            chars_read[NORMAL2] = 8;
            chars_read[SCROLL0] = 40;
            chars_read[SCROLL1] = 20;
            chars_read[SCROLL2] = 10;
            chars_displayed[NORMAL0] = 32;
            chars_displayed[NORMAL1] = 16;
            chars_displayed[NORMAL2] = 8;
            x_min[NORMAL0] = 32;
            x_min[NORMAL1] = 32;
            x_min[NORMAL2] = 32;
            ch_offset[NORMAL0] = 0;
            ch_offset[NORMAL1] = 0;
            ch_offset[NORMAL2] = 0;
            font_cycles[NORMAL0] = load_cycles[NORMAL0] = 32;
            font_cycles[NORMAL1] = load_cycles[NORMAL1] = 16;
            load_cycles[NORMAL2] = 8;
            before_cycles[NORMAL0] = BEFORE_CYCLES;
            before_cycles[SCROLL0] = BEFORE_CYCLES + 8;
            extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES;
            extra_cycles[SCROLL0] = 8 + BEFORE_CYCLES + 8;
            left_border_chars = 8 - LCHOP;
            right_border_start = (ATARI_WIDTH - 64) / 2;
            break;
        case 0x02:
            chars_read[NORMAL0] = 40;
            chars_read[NORMAL1] = 20;
            chars_read[NORMAL2] = 10;
            chars_read[SCROLL0] = 48;
            chars_read[SCROLL1] = 24;
            chars_read[SCROLL2] = 12;
            chars_displayed[NORMAL0] = 40;
            chars_displayed[NORMAL1] = 20;
            chars_displayed[NORMAL2] = 10;
            x_min[NORMAL0] = 16;
            x_min[NORMAL1] = 16;
            x_min[NORMAL2] = 16;
            ch_offset[NORMAL0] = 0;
            ch_offset[NORMAL1] = 0;
            ch_offset[NORMAL2] = 0;
            font_cycles[NORMAL0] = load_cycles[NORMAL0] = 40;
            font_cycles[NORMAL1] = load_cycles[NORMAL1] = 20;
            load_cycles[NORMAL2] = 10;
            before_cycles[NORMAL0] = BEFORE_CYCLES + 8;
            before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
            extra_cycles[NORMAL0] = 8 + BEFORE_CYCLES + 8;
            extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
            left_border_chars = 4 - LCHOP;
            right_border_start = (ATARI_WIDTH - 32) / 2;
            break;
        case 0x03:
            chars_read[NORMAL0] = 48;
            chars_read[NORMAL1] = 24;
            chars_read[NORMAL2] = 12;
            chars_read[SCROLL0] = 48;
            chars_read[SCROLL1] = 24;
            chars_read[SCROLL2] = 12;
            chars_displayed[NORMAL0] = 42;
            chars_displayed[NORMAL1] = 22;
            chars_displayed[NORMAL2] = 12;
            x_min[NORMAL0] = 12;
            x_min[NORMAL1] = 8;
            x_min[NORMAL2] = 0;
            ch_offset[NORMAL0] = 3;
            ch_offset[NORMAL1] = 1;
            ch_offset[NORMAL2] = 0;
            font_cycles[NORMAL0] = load_cycles[NORMAL0] = 47;
            font_cycles[NORMAL1] = load_cycles[NORMAL1] = 24;
            load_cycles[NORMAL2] = 12;
            before_cycles[NORMAL0] = BEFORE_CYCLES + 16;
            before_cycles[SCROLL0] = BEFORE_CYCLES + 16;
            extra_cycles[NORMAL0] = 7 + BEFORE_CYCLES + 16;
            extra_cycles[SCROLL0] = 7 + BEFORE_CYCLES + 16;
            left_border_chars = 3 - LCHOP;
            right_border_start = (ATARI_WIDTH - 8) / 2;
            break;
        }

        missile_dma_enabled = (byte & 0x0c);    /* no player dma without missile */
        player_dma_enabled = (byte & 0x08);
        singleline = (byte & 0x10);
        player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
        missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);

        byte = HSCROL;  /* update horizontal scroll data */
/* ******* FALLTHROUGH ******* */
    case _HSCROL:
        HSCROL = byte &= 0x0f;
        if (DMACTL & 3) {
            chars_displayed[SCROLL0] = chars_displayed[NORMAL0];
            ch_offset[SCROLL0] = 4 - (byte >> 2);
            x_min[SCROLL0] = x_min[NORMAL0];
            if (byte & 3) {
                x_min[SCROLL0] += (byte & 3) - 4;
                chars_displayed[SCROLL0]++;
                ch_offset[SCROLL0]--;
            }
            chars_displayed[SCROLL2] = chars_displayed[NORMAL2];
            if ((DMACTL & 3) == 3) {    /* wide playfield */
                ch_offset[SCROLL0]--;
                if (byte == 4 || byte == 12)
                    chars_displayed[SCROLL1] = 21;
                else
                    chars_displayed[SCROLL1] = 22;
                if (byte <= 4) {
                    x_min[SCROLL1] = byte + 8;
                    ch_offset[SCROLL1] = 1;
                }
                else if (byte <= 12) {
                    x_min[SCROLL1] = byte;
                    ch_offset[SCROLL1] = 0;
                }
                else {
                    x_min[SCROLL1] = byte - 8;
                    ch_offset[SCROLL1] = -1;
                }
                /* technically, the part below is wrong */
                /* scrolling in mode 8,9 with HSCROL=13,14,15 */
                /* will set x_min=13,14,15 > 4*LCHOP = 12 */
                /* so that nothing is drawn on the far left side */
                /* of the screen.  We could fix this, but only */
                /* by setting x_min to be negative. */
                x_min[SCROLL2] = byte;
                ch_offset[SCROLL2] = 0;
            }
            else {
                chars_displayed[SCROLL1] = chars_displayed[NORMAL1];
                ch_offset[SCROLL1] = 2 - (byte >> 3);
                x_min[SCROLL1] = x_min[NORMAL0];
                if (byte) {
                    if (byte & 7) {
                        x_min[SCROLL1] += (byte & 7) - 8;
                        chars_displayed[SCROLL1]++;
                        ch_offset[SCROLL1]--;
                    }
                    x_min[SCROLL2] = x_min[NORMAL2] + byte - 16;
                    chars_displayed[SCROLL2]++;
                    ch_offset[SCROLL2] = 0;
                }
                else {
                    x_min[SCROLL2] = x_min[NORMAL2];
                    ch_offset[SCROLL2] = 1;
                }
            }

            if (DMACTL & 2) {       /* normal & wide playfield */
                load_cycles[SCROLL0] = 47 - (byte >> 2);
                font_cycles[SCROLL0] = (47 * 4 + 1 - byte) >> 2;
                load_cycles[SCROLL1] = (24 * 8 + 3 - byte) >> 3;
                font_cycles[SCROLL1] = (24 * 8 + 1 - byte) >> 3;
                load_cycles[SCROLL2] = byte < 0xc ? 12 : 11;
            }
            else {                  /* narrow playfield */
                font_cycles[SCROLL0] = load_cycles[SCROLL0] = 40;
                font_cycles[SCROLL1] = load_cycles[SCROLL1] = 20;
                load_cycles[SCROLL2] = 16;
            }
        }
        break;
    case _VSCROL:
        VSCROL = byte & 0x0f;
        if (vscrol_off) {
            lastline = VSCROL;
            if (XPOS < VSCOF_C)
                need_dl = dctr == lastline;
        }
        break;
    case _PMBASE:
        PMBASE = byte;
        pmbase_d = (byte & 0xfc) << 8;
        pmbase_s = pmbase_d & 0xf8ff;
        break;
    case _CHACTL:
        invert_mask = byte & 2 ? 0x80 : 0;
        blank_mask = byte & 1 ? 0xe0 : 0x60;
        if ((CHACTL ^ byte) & 4) {
            chbase_20 ^= 7;
        }
        CHACTL = byte;
        break;
    case _CHBASE:
        CHBASE = byte;
        chbase_20 = (byte & 0xfe) << 8;
        if (CHACTL & 4)
            chbase_20 ^= 7;
        break;

    case _WSYNC:
        if (xpos <= WSYNC_C && xpos_limit >= WSYNC_C)
            xpos = WSYNC_C;
        else {
            wsync_halt = TRUE;
            xpos = xpos_limit;
        }
        break;
    case _NMIEN:
        NMIEN = byte;
        break;
    case _NMIRES:
        NMIST = 0x1f;
        break;
    default:
        break;
    }
}


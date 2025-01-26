/*
 * GTIA.C contains the emulation of the Core GTIA graphics chip on the Atari 800.
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
 * gtia.c - GTIA chip emulation
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
#include <nds.h>
#include "atari.h"
#include <string.h>
#include "antic.h"
#include "esc.h"
#include "gtia.h"
#include "input.h"
#include "pokeysnd.h"

/* GTIA Registers ---------------------------------------------------------- */

UBYTE M0PL __attribute__((section(".dtcm")));
UBYTE M1PL __attribute__((section(".dtcm")));
UBYTE M2PL __attribute__((section(".dtcm")));
UBYTE M3PL __attribute__((section(".dtcm")));
UBYTE P0PL __attribute__((section(".dtcm")));
UBYTE P1PL __attribute__((section(".dtcm")));
UBYTE P2PL __attribute__((section(".dtcm")));
UBYTE P3PL __attribute__((section(".dtcm")));
UBYTE HPOSP0 __attribute__((section(".dtcm")));
UBYTE HPOSP1 __attribute__((section(".dtcm")));
UBYTE HPOSP2 __attribute__((section(".dtcm")));
UBYTE HPOSP3 __attribute__((section(".dtcm")));
UBYTE HPOSM0 __attribute__((section(".dtcm")));
UBYTE HPOSM1 __attribute__((section(".dtcm")));
UBYTE HPOSM2 __attribute__((section(".dtcm")));
UBYTE HPOSM3 __attribute__((section(".dtcm")));
UBYTE SIZEP0 __attribute__((section(".dtcm")));
UBYTE SIZEP1 __attribute__((section(".dtcm")));
UBYTE SIZEP2 __attribute__((section(".dtcm")));
UBYTE SIZEP3 __attribute__((section(".dtcm")));
UBYTE SIZEM __attribute__((section(".dtcm")));
UBYTE GRAFP0 __attribute__((section(".dtcm")));
UBYTE GRAFP1 __attribute__((section(".dtcm")));
UBYTE GRAFP2 __attribute__((section(".dtcm")));
UBYTE GRAFP3 __attribute__((section(".dtcm")));
UBYTE GRAFM __attribute__((section(".dtcm")));
UBYTE COLPM0 __attribute__((section(".dtcm")));
UBYTE COLPM1 __attribute__((section(".dtcm")));
UBYTE COLPM2 __attribute__((section(".dtcm")));
UBYTE COLPM3 __attribute__((section(".dtcm")));
UBYTE COLPF0 __attribute__((section(".dtcm")));
UBYTE COLPF1 __attribute__((section(".dtcm")));
UBYTE COLPF2 __attribute__((section(".dtcm")));
UBYTE COLPF3 __attribute__((section(".dtcm")));
UBYTE COLBK __attribute__((section(".dtcm")));
UBYTE PRIOR __attribute__((section(".dtcm")));
UBYTE VDELAY __attribute__((section(".dtcm")));
UBYTE GRACTL __attribute__((section(".dtcm")));
UBYTE POTENA __attribute__((section(".dtcm")));

/* Internal GTIA state ----------------------------------------------------- */

int atari_speaker       __attribute__((section(".dtcm")));
int consol_index        __attribute__((section(".dtcm"))) = 0;
UBYTE consol_table[3]   __attribute__((section(".dtcm")));
UBYTE consol_mask       __attribute__((section(".dtcm")));
UBYTE TRIG[4]           __attribute__((section(".dtcm")));
UBYTE TRIG_latch[4]     __attribute__((section(".dtcm")));

void set_prior(UBYTE byte);         /* in antic.c */

/* Player/Missile stuff ---------------------------------------------------- */

/* change to 0x00 to disable collisions */
#define collisions_mask_missile_playfield  0x0f
#define collisions_mask_player_playfield   0x0f
#define collisions_mask_missile_player     0x0f
#define collisions_mask_player_player      0x0f

#ifdef NEW_CYCLE_EXACT
/* temporary collision registers for the current scanline only */
UBYTE P1PL_T __attribute__((section(".dtcm")));
UBYTE P2PL_T __attribute__((section(".dtcm")));
UBYTE P3PL_T __attribute__((section(".dtcm")));
UBYTE M0PL_T __attribute__((section(".dtcm")));
UBYTE M1PL_T __attribute__((section(".dtcm")));
UBYTE M2PL_T __attribute__((section(".dtcm")));
UBYTE M3PL_T __attribute__((section(".dtcm")));
/* If partial collisions have been generated during a scanline, this
 * is the position of the up-to-date collision point , otherwise it is 0
 */
short int collision_curpos __attribute__((section(".dtcm")));
/* if hitclr has been written to during a scanline, this is the position
 * within pm_scaline at which it was written to, and collisions should
 * only be generated from this point on, otherwise it is 0
 */
short int hitclr_pos __attribute__((section(".dtcm")));
#else
#define P1PL_T P1PL
#define P2PL_T P2PL
#define P3PL_T P3PL
#define M0PL_T M0PL
#define M1PL_T M1PL
#define M2PL_T M2PL
#define M3PL_T M3PL
#endif /* NEW_CYCLE_EXACT */


extern UBYTE player_dma_enabled;
extern UBYTE missile_dma_enabled;
extern UBYTE player_gra_enabled;
extern UBYTE missile_gra_enabled;
extern UBYTE player_flickering;
extern UBYTE missile_flickering;

UBYTE *hposp_ptr[4] __attribute__((section(".dtcm")));
UBYTE *hposm_ptr[4] __attribute__((section(".dtcm")));
ULONG hposp_mask[4] __attribute__((section(".dtcm")));

ULONG *grafp_lookup = (ULONG*)0x068A0000;   // Using 4K here for lookup table... ULONG[4][256]

ULONG *grafp_ptr[4] __attribute__((section(".dtcm")));
int global_sizem[4] __attribute__((section(".dtcm")));

UBYTE PM_Width[4]  __attribute__((section(".dtcm"))) = {1, 2, 1, 4};

/* Meaning of bits in pm_scanline:
bit 0 - Player 0
bit 1 - Player 1
bit 2 - Player 2
bit 3 - Player 3
bit 4 - Missile 0
bit 5 - Missile 1
bit 6 - Missile 2
bit 7 - Missile 3
*/

UBYTE pm_scanline[ATARI_WIDTH / 2 + 8] __attribute__((section(".dtcm")));   /* there's a byte for every *pair* of pixels */
UBYTE pm_dirty  __attribute__((section(".dtcm"))) = TRUE;

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

#define PF0PM (*(UBYTE *) &cl_lookup[C_PF0 | C_COLLS])
#define PF1PM (*(UBYTE *) &cl_lookup[C_PF1 | C_COLLS])
#define PF2PM (*(UBYTE *) &cl_lookup[C_PF2 | C_COLLS])
#define PF3PM (*(UBYTE *) &cl_lookup[C_PF3 | C_COLLS])

/* Colours ----------------------------------------------------------------- */

void setup_gtia9_11(void) {
    int i;
    ULONG count9 = 0;
    ULONG count11 = 0;
    lookup_gtia11[0] = lookup_gtia9[0] & 0xf0f0f0f0;
    for (i = 1; i < 16; i++) {
        lookup_gtia9[i] = lookup_gtia9[0] | (count9 += 0x01010101);
        lookup_gtia11[i] = lookup_gtia9[0] | (count11 += 0x10101010);
    }
}


/* Initialization ---------------------------------------------------------- */

void GTIA_Initialise(void) {
    int i;
    for (i = 0; i < 256; i++) {
        int tmp = i + 0x100;
        ULONG grafp1 = 0;
        ULONG grafp2 = 0;
        ULONG grafp4 = 0;
        do {
            grafp1 <<= 1;
            grafp2 <<= 2;
            grafp4 <<= 4;
            if (tmp & 1) {
                grafp1++;
                grafp2 += 3;
                grafp4 += 15;
            }
            tmp >>= 1;
        } while (tmp != 1);
        grafp_lookup[2*256+i] = grafp_lookup[0*256+i] = grafp1;
        grafp_lookup[1*256+i] = grafp2;
        grafp_lookup[3*256+i] = grafp4;
    }
    memset(cl_lookup, COLOUR_BLACK, sizeof(cl_lookup));
    for (i = 0; i < 32; i++)
        GTIA_PutByte((UWORD) i, 0);
  POTENA=0;
}

#ifdef NEW_CYCLE_EXACT

/* generate updated PxPL and MxPL for part of a scanline */
/* slow, but should be called rarely */
void generate_partial_pmpl_colls(int l, int r)
{
    int i;
    if (r < 0 || l >= (int) sizeof(pm_scanline) / (int) sizeof(pm_scanline[0]))
        return;
    if (r >= (int) sizeof(pm_scanline) / (int) sizeof(pm_scanline[0])) {
        r = (int) sizeof(pm_scanline) / (int) sizeof(pm_scanline[0]);
    }
    if (l < 0)
        l = 0;

    for (i = l; i <= r; i++) {
        UBYTE p = pm_scanline[i];
/* It is possible that some bits are set in PxPL/MxPL here, which would
 * not otherwise be set ever in new_pm_scanline.  This is because the
 * player collisions are always generated in order in new_pm_scanline.
 * However this does not cause any problem because we never use those bits
 * of PxPL/MxPL in the collision reading code.
 */
        P1PL |= (p & (1 << 1)) ?  p : 0;
        P2PL |= (p & (1 << 2)) ?  p : 0;
        P3PL |= (p & (1 << 3)) ?  p : 0;
        M0PL |= (p & (0x10 << 0)) ?  p : 0;
        M1PL |= (p & (0x10 << 1)) ?  p : 0;
        M2PL |= (p & (0x10 << 2)) ?  p : 0;
        M3PL |= (p & (0x10 << 3)) ?  p : 0;
    }

}

/* update pm->pl collisions for a partial scanline */
void update_partial_pmpl_colls(void)
{
    int l = collision_curpos;
    int r = XPOS * 2 - 37;
    generate_partial_pmpl_colls(l, r);
    collision_curpos = r;
}

/* update pm-> pl collisions at the end of a scanline */
void update_pmpl_colls(void)
{
    if (hitclr_pos != 0){
        generate_partial_pmpl_colls(hitclr_pos,
                sizeof(pm_scanline) / sizeof(pm_scanline[0]) - 1);
/* If hitclr was written to, then only part of pm_scanline should be used
 * for collisions */

    }
    else {
/* otherwise the whole of pm_scaline can be used for collisions.  This will
 * update the collision registers based on the generated collisions for the
 * current line */
        P1PL |= P1PL_T;
        P2PL |= P2PL_T;
        P3PL |= P3PL_T;
        M0PL |= M0PL_T;
        M1PL |= M1PL_T;
        M2PL |= M2PL_T;
        M3PL |= M3PL_T;
    }
    collision_curpos = 0;
    hitclr_pos = 0;
}

#else
#define update_partial_pmpl_colls()
#endif /* NEW_CYCLE_EXACT */


/* Prepare PMG scanline ---------------------------------------------------- */

ITCM_CODE void new_pm_scanline(void)
{
#ifdef NEW_CYCLE_EXACT
/* reset temporary pm->pl collisions */
    P1PL_T = P2PL_T = P3PL_T = 0;
    M0PL_T = M1PL_T = M2PL_T = M3PL_T = 0;
#endif /* NEW_CYCLE_EXACT */
/* Clear if necessary */
    if (pm_dirty) {
        memset(pm_scanline, 0, ATARI_WIDTH / 2);
        pm_dirty = FALSE;
    }

/* Draw Players */

#define DO_PLAYER(n)    if (GRAFP##n) {                     \
    ULONG grafp = grafp_ptr[n][GRAFP##n] & hposp_mask[n];   \
    if (grafp) {                                            \
        UBYTE *ptr = hposp_ptr[n];                          \
        pm_dirty = TRUE;                                    \
        do {                                                \
            if (grafp & 1)                                  \
                P##n##PL_T |= *ptr |= 1 << n;               \
            ptr++;                                          \
            grafp >>= 1;                                    \
        } while (grafp);                                    \
    }                                                       \
}

    /* optimized DO_PLAYER(0): pm_scanline is clear and P0PL is unused */
    if (GRAFP0) {
        ULONG grafp = grafp_ptr[0][GRAFP0] & hposp_mask[0];
        if (grafp) {
            UBYTE *ptr = hposp_ptr[0];
            pm_dirty = TRUE;
            do {
                if (grafp & 1)
                    *ptr = 1;
                ptr++;
                grafp >>= 1;
            } while (grafp);
        }
    }

    DO_PLAYER(1)
    DO_PLAYER(2)
    DO_PLAYER(3)

/* Draw Missiles */

#define DO_MISSILE(n,p,m,r,l)   if (GRAFM & m) {    \
    int j = global_sizem[n];                        \
    UBYTE *ptr = hposm_ptr[n];                      \
    if (GRAFM & r) {                                \
        if (GRAFM & l)                              \
            j <<= 1;                                \
    }                                               \
    else                                            \
        ptr += j;                                   \
    if (ptr < pm_scanline + 2) {                    \
        j += ptr - pm_scanline - 2;                 \
        ptr = pm_scanline + 2;                      \
    }                                               \
    else if (ptr + j > pm_scanline + ATARI_WIDTH / 2 - 2)   \
        j = pm_scanline + ATARI_WIDTH / 2 - 2 - ptr;        \
    if (j > 0)                                      \
        do                                          \
            M##n##PL_T |= *ptr++ |= p;              \
        while (--j);                                \
}

    if (GRAFM) {
        pm_dirty = TRUE;
        DO_MISSILE(3, 0x80, 0xc0, 0x80, 0x40)
        DO_MISSILE(2, 0x40, 0x30, 0x20, 0x10)
        DO_MISSILE(1, 0x20, 0x0c, 0x08, 0x04)
        DO_MISSILE(0, 0x10, 0x03, 0x02, 0x01)
    }
}

/* GTIA registers ---------------------------------------------------------- */

void GTIA_Frame(void) {
    int consol = key_consol | 0x08;

    consol_table[0] = consol;
    consol_table[1] = consol_table[2] &= consol;

    if (GRACTL & 4) {
        TRIG_latch[0] &= TRIG[0];
        TRIG_latch[1] &= TRIG[1];
        TRIG_latch[2] &= TRIG[2];
        TRIG_latch[3] &= TRIG[3];
    }
}

UBYTE GTIA_GetByte(UWORD addr)
{
    switch (addr & 0x1f) {
    case _M0PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return (((PF0PM & 0x10) >> 4)
              + ((PF1PM & 0x10) >> 3)
              + ((PF2PM & 0x10) >> 2)
              + ((PF3PM & 0x10) >> 1)) & collisions_mask_missile_playfield;
    case _M1PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return (((PF0PM & 0x20) >> 5)
              + ((PF1PM & 0x20) >> 4)
              + ((PF2PM & 0x20) >> 3)
              + ((PF3PM & 0x20) >> 2)) & collisions_mask_missile_playfield;
    case _M2PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return (((PF0PM & 0x40) >> 6)
              + ((PF1PM & 0x40) >> 5)
              + ((PF2PM & 0x40) >> 4)
              + ((PF3PM & 0x40) >> 3)) & collisions_mask_missile_playfield;
    case _M3PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return (((PF0PM & 0x80) >> 7)
              + ((PF1PM & 0x80) >> 6)
              + ((PF2PM & 0x80) >> 5)
              + ((PF3PM & 0x80) >> 4)) & collisions_mask_missile_playfield;
    case _P0PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return ((PF0PM & 0x01)
              + ((PF1PM & 0x01) << 1)
              + ((PF2PM & 0x01) << 2)
              + ((PF3PM & 0x01) << 3)) & collisions_mask_player_playfield;
    case _P1PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return (((PF0PM & 0x02) >> 1)
              + (PF1PM & 0x02)
              + ((PF2PM & 0x02) << 1)
              + ((PF3PM & 0x02) << 2)) & collisions_mask_player_playfield;
    case _P2PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return (((PF0PM & 0x04) >> 2)
              + ((PF1PM & 0x04) >> 1)
              + (PF2PM & 0x04)
              + ((PF3PM & 0x04) << 1)) & collisions_mask_player_playfield;
    case _P3PF:
#ifdef NEW_CYCLE_EXACT
        if (DRAWING_SCREEN) {update_scanline();}
#endif
        return (((PF0PM & 0x08) >> 3)
              + ((PF1PM & 0x08) >> 2)
              + ((PF2PM & 0x08) >> 1)
              + (PF3PM & 0x08)) & collisions_mask_player_playfield;
    case _M0PL:
        update_partial_pmpl_colls();
        return M0PL & collisions_mask_missile_player;
    case _M1PL:
        update_partial_pmpl_colls();
        return M1PL & collisions_mask_missile_player;
    case _M2PL:
        update_partial_pmpl_colls();
        return M2PL & collisions_mask_missile_player;
    case _M3PL:
        update_partial_pmpl_colls();
        return M3PL & collisions_mask_missile_player;
    case _P0PL:
        update_partial_pmpl_colls();
        return (((P1PL & 0x01) << 1)  /* mask in player 1 */
              + ((P2PL & 0x01) << 2)  /* mask in player 2 */
              + ((P3PL & 0x01) << 3)) /* mask in player 3 */
             & collisions_mask_player_player;
    case _P1PL:
        update_partial_pmpl_colls();
        return ((P1PL & 0x01)         /* mask in player 0 */
              + ((P2PL & 0x02) << 1)  /* mask in player 2 */
              + ((P3PL & 0x02) << 2)) /* mask in player 3 */
             & collisions_mask_player_player;
    case _P2PL:
        update_partial_pmpl_colls();
        return ((P2PL & 0x03)         /* mask in player 0 and 1 */
              + ((P3PL & 0x04) << 1)) /* mask in player 3 */
             & collisions_mask_player_player;
    case _P3PL:
        update_partial_pmpl_colls();
        return (P3PL & 0x07)          /* mask in player 0,1, and 2 */
             & collisions_mask_player_player;
    case _TRIG0:
        return TRIG[0] & TRIG_latch[0];
    case _TRIG1:
        return TRIG[1] & TRIG_latch[1];
    case _TRIG2:
        return TRIG[2] & TRIG_latch[2];
    case _TRIG3:
        return TRIG[3] & TRIG_latch[3];
    case _PAL:
        return (myConfig.tv_type == TV_PAL) ? 0x01 : 0x0f;
    case _CONSOL:
        {
            UBYTE byte = consol_table[consol_index] & consol_mask;
            if (consol_index > 0) {
                consol_index--;
                if (consol_index == 0 && CASSETTE_hold_start) {
                    /* press Space after Start to start cassette boot */
                    CASSETTE_press_space = 1;
                    CASSETTE_hold_start = CASSETTE_hold_start_on_reboot;
                }
            }
            return byte;
        }
    default:
        break;
    }

    return 0xf;
}

#ifdef NEW_CYCLE_EXACT
#define UPDATE_PM_CYCLE_EXACT if(DRAWING_SCREEN) new_pm_scanline();
#else
#define UPDATE_PM_CYCLE_EXACT
#endif


void GTIA_PutByte(UWORD addr, UBYTE byte)
{
    UWORD cword;
    UWORD cword2;
#ifdef NEW_CYCLE_EXACT
    int x; /* the cycle-exact update position in pm_scanline */
    if (DRAWING_SCREEN) {
        if ((addr & 0x1f) != _PRIOR) {
            update_scanline();
        } else {
            update_scanline_prior(byte);
        }
    }
#endif
    switch (addr & 0x1f) {
    case _CONSOL:
        atari_speaker = !(byte & 0x08);
        consol_mask = (~byte) & 0x0f;
    POTENA = byte & 0x04;
        break;

    case _COLBK:
        COLBK = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_BAK] = cword;
        if (cword != (UWORD) (lookup_gtia9[0]) ) {
            lookup_gtia9[0] = cword + (cword << 16);
            if (PRIOR & 0x40)
                setup_gtia9_11();
        }
        break;
    case _COLPF0:
        COLPF0 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PF0] = cword;
        if ((PRIOR & 1) == 0) {
            cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF0 | C_PM2] = cword;
            if ((PRIOR & 3) == 0) {
                if (PRIOR & 0xf) {
                    cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF0 | C_PM0] = cword;
                    if ((PRIOR & 0xf) == 0xc)
                        cl_lookup[C_PF0 | C_PM0123] = cl_lookup[C_PF0 | C_PM123] = cl_lookup[C_PF0 | C_PM023] = cword;
                }
                else
                    cl_lookup[C_PF0 | C_PM01] = (cl_lookup[C_PF0 | C_PM0] = cword | cl_lookup[C_PM0]) | (cl_lookup[C_PF0 | C_PM1] = cword | cl_lookup[C_PM1]);
            }
            if ((PRIOR & 0xf) >= 0xa)
                cl_lookup[C_PF0 | C_PM25] = cword;
        }
        break;
    case _COLPF1:
        COLPF1 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PF1] = cword;
        if ((PRIOR & 1) == 0) {
            cl_lookup[C_PF1 | C_PM23] = cl_lookup[C_PF1 | C_PM3] = cl_lookup[C_PF1 | C_PM2] = cword;
            if ((PRIOR & 3) == 0) {
                if (PRIOR & 0xf) {
                    cl_lookup[C_PF1 | C_PM01] = cl_lookup[C_PF1 | C_PM1] = cl_lookup[C_PF1 | C_PM0] = cword;
                    if ((PRIOR & 0xf) == 0xc)
                        cl_lookup[C_PF1 | C_PM0123] = cl_lookup[C_PF1 | C_PM123] = cl_lookup[C_PF1 | C_PM023] = cword;
                }
                else
                    cl_lookup[C_PF1 | C_PM01] = (cl_lookup[C_PF1 | C_PM0] = cword | cl_lookup[C_PM0]) | (cl_lookup[C_PF1 | C_PM1] = cword | cl_lookup[C_PM1]);
            }
        }
        ((UBYTE *)hires_lookup_l)[0x80] = ((UBYTE *)hires_lookup_l)[0x41] = (UBYTE)
            (hires_lookup_l[0x60] = cword & 0xf0f);
        break;
    case _COLPF2:
        COLPF2 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PF2] = cword;
        if (PRIOR & 4)
            cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF2 | C_PM0] = cword;
        if ((PRIOR & 9) == 0) {
            if (PRIOR & 0xf)
                cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF2 | C_PM2] = cword;
            else
                cl_lookup[C_PF2 | C_PM23] = (cl_lookup[C_PF2 | C_PM2] = cword | cl_lookup[C_PM2]) | (cl_lookup[C_PF2 | C_PM3] = cword | cl_lookup[C_PM3]);
        }
        break;
    case _COLPF3:
        COLPF3 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PF3] = cword;
        if (PRIOR & 4)
            cl_lookup[C_PF3 | C_PM01] = cl_lookup[C_PF3 | C_PM1] = cl_lookup[C_PF3 | C_PM0] = cword;
        if ((PRIOR & 9) == 0) {
            if (PRIOR & 0xf)
                cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM3] = cl_lookup[C_PF3 | C_PM2] = cword;
            else {
                cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2] = cword | cl_lookup[C_PM2];
                cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3] = cword | cl_lookup[C_PM3];
                cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = cl_lookup[C_PF3 | C_PM2] | cl_lookup[C_PF3 | C_PM3];
                cl_lookup[C_PF0 | C_PM235] = cl_lookup[C_PF0 | C_PM35] = cl_lookup[C_PF0 | C_PM25] =
                cl_lookup[C_PF1 | C_PM235] = cl_lookup[C_PF1 | C_PM35] = cl_lookup[C_PF1 | C_PM25] = cword;
            }
        }
        break;
    case _COLPM0:
        COLPM0 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PM023] = cl_lookup[C_PM0] = cword;
        cl_lookup[C_PM0123] = cl_lookup[C_PM01] = cword2 = cword | cl_lookup[C_PM1];
        if ((PRIOR & 4) == 0) {
            cl_lookup[C_PF2 | C_PM0] = cl_lookup[C_PF3 | C_PM0] = cword;
            cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF3 | C_PM01] = cword2;
            if ((PRIOR & 0xc) == 0) {
                if (PRIOR & 3) {
                    cl_lookup[C_PF0 | C_PM0] = cl_lookup[C_PF1 | C_PM0] = cword;
                    cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF1 | C_PM01] = cword2;
                }
                else {
                    cl_lookup[C_PF0 | C_PM0] = cword | cl_lookup[C_PF0];
                    cl_lookup[C_PF1 | C_PM0] = cword | cl_lookup[C_PF1];
                    cl_lookup[C_PF0 | C_PM01] = cword2 | cl_lookup[C_PF0];
                    cl_lookup[C_PF1 | C_PM01] = cword2 | cl_lookup[C_PF1];
                }
            }
        }
        break;
    case _COLPM1:
        COLPM1 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PM123] = cl_lookup[C_PM1] = cword;
        cl_lookup[C_PM0123] = cl_lookup[C_PM01] = cword2 = cword | cl_lookup[C_PM0];
        if ((PRIOR & 4) == 0) {
            cl_lookup[C_PF2 | C_PM1] = cl_lookup[C_PF3 | C_PM1] = cword;
            cl_lookup[C_PF2 | C_PM01] = cl_lookup[C_PF3 | C_PM01] = cword2;
            if ((PRIOR & 0xc) == 0) {
                if (PRIOR & 3) {
                    cl_lookup[C_PF0 | C_PM1] = cl_lookup[C_PF1 | C_PM1] = cword;
                    cl_lookup[C_PF0 | C_PM01] = cl_lookup[C_PF1 | C_PM01] = cword2;
                }
                else {
                    cl_lookup[C_PF0 | C_PM1] = cword | cl_lookup[C_PF0];
                    cl_lookup[C_PF1 | C_PM1] = cword | cl_lookup[C_PF1];
                    cl_lookup[C_PF0 | C_PM01] = cword2 | cl_lookup[C_PF0];
                    cl_lookup[C_PF1 | C_PM01] = cword2 | cl_lookup[C_PF1];
                }
            }
        }
        break;
    case _COLPM2:
        COLPM2 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PM2] = cword;
        cl_lookup[C_PM23] = cword2 = cword | cl_lookup[C_PM3];
        if (PRIOR & 1) {
            cl_lookup[C_PF0 | C_PM2] = cl_lookup[C_PF1 | C_PM2] = cword;
            cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF1 | C_PM23] = cword2;
        }
        if ((PRIOR & 6) == 0) {
            if (PRIOR & 9) {
                cl_lookup[C_PF2 | C_PM2] = cl_lookup[C_PF3 | C_PM2] = cword;
                cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF3 | C_PM23] = cword2;
            }
            else {
                cl_lookup[C_PF2 | C_PM2] = cword | cl_lookup[C_PF2];
                cl_lookup[C_PF3 | C_PM25] = cl_lookup[C_PF2 | C_PM25] = cl_lookup[C_PM25] = cl_lookup[C_PF3 | C_PM2] = cword | cl_lookup[C_PF3];
                cl_lookup[C_PF2 | C_PM23] = cword2 | cl_lookup[C_PF2];
                cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = cword2 | cl_lookup[C_PF3];
            }
        }
        break;
    case _COLPM3:
        COLPM3 = byte &= 0xfe;
        COLOUR_TO_WORD(cword,byte);
        cl_lookup[C_PM3] = cword;
        cl_lookup[C_PM23] = cword2 = cword | cl_lookup[C_PM2];
        if (PRIOR & 1) {
            cl_lookup[C_PF0 | C_PM3] = cl_lookup[C_PF1 | C_PM3] = cword;
            cl_lookup[C_PF0 | C_PM23] = cl_lookup[C_PF1 | C_PM23] = cword2;
        }
        if ((PRIOR & 6) == 0) {
            if (PRIOR & 9) {
                cl_lookup[C_PF2 | C_PM3] = cl_lookup[C_PF3 | C_PM3] = cword;
                cl_lookup[C_PF2 | C_PM23] = cl_lookup[C_PF3 | C_PM23] = cword2;
            }
            else {
                cl_lookup[C_PF2 | C_PM3] = cword | cl_lookup[C_PF2];
                cl_lookup[C_PF3 | C_PM35] = cl_lookup[C_PF2 | C_PM35] = cl_lookup[C_PM35] = cl_lookup[C_PF3 | C_PM3] = cword | cl_lookup[C_PF3];
                cl_lookup[C_PF2 | C_PM23] = cword2 | cl_lookup[C_PF2];
                cl_lookup[C_PF3 | C_PM235] = cl_lookup[C_PF2 | C_PM235] = cl_lookup[C_PM235] = cl_lookup[C_PF3 | C_PM23] = cword2 | cl_lookup[C_PF3];
            }
        }
        break;
    case _GRAFM:
        GRAFM = byte;
        UPDATE_PM_CYCLE_EXACT
        break;

#ifdef NEW_CYCLE_EXACT
#define CYCLE_EXACT_GRAFP(n) x = XPOS * 2 - 3;\
    if (HPOSP##n >= x) {\
    /* hpos right of x */\
        /* redraw */  \
        UPDATE_PM_CYCLE_EXACT\
    }
#else
#define CYCLE_EXACT_GRAFP(n)
#endif /* NEW_CYCLE_EXACT */


#define DO_GRAFP(n) case _GRAFP##n:\
    GRAFP##n = byte;\
    CYCLE_EXACT_GRAFP(n);\
    break;

    DO_GRAFP(0)
    DO_GRAFP(1)
    DO_GRAFP(2)
    DO_GRAFP(3)

    case _HITCLR:
        M0PL = M1PL = M2PL = M3PL = 0;
        P0PL = P1PL = P2PL = P3PL = 0;
        PF0PM = PF1PM = PF2PM = PF3PM = 0;
#ifdef NEW_CYCLE_EXACT
        hitclr_pos = XPOS * 2 - 37;
        collision_curpos = hitclr_pos;
#endif
        break;
/* TODO: cycle-exact missile HPOS, GRAF, SIZE */
/* this is only an approximation */
    case _HPOSM0:
        HPOSM0 = byte;
        hposm_ptr[0] = pm_scanline + byte - 0x20;
        UPDATE_PM_CYCLE_EXACT
        break;
    case _HPOSM1:
        HPOSM1 = byte;
        hposm_ptr[1] = pm_scanline + byte - 0x20;
        UPDATE_PM_CYCLE_EXACT
        break;
    case _HPOSM2:
        HPOSM2 = byte;
        hposm_ptr[2] = pm_scanline + byte - 0x20;
        UPDATE_PM_CYCLE_EXACT
        break;
    case _HPOSM3:
        HPOSM3 = byte;
        hposm_ptr[3] = pm_scanline + byte - 0x20;
        UPDATE_PM_CYCLE_EXACT
        break;

#ifdef NEW_CYCLE_EXACT
#define CYCLE_EXACT_HPOSP(n) x = XPOS * 2 - 1;\
    if (HPOSP##n < x && byte < x) {\
    /* case 1: both left of x */\
        /* do nothing */\
    }\
    else if (HPOSP##n >= x && byte >= x ) {\
    /* case 2: both right of x */\
        /* redraw, clearing first */\
        UPDATE_PM_CYCLE_EXACT\
    }\
    else if (HPOSP##n <x && byte >= x) {\
    /* case 3: new value is right, old value is left */\
        /* redraw without clearning first */\
        /* note: a hack, we can get away with it unless another change occurs */\
        /* before the original copy that wasn't erased due to changing */\
        /* pm_dirty is drawn */\
        pm_dirty = FALSE;\
        UPDATE_PM_CYCLE_EXACT\
        pm_dirty = TRUE; /* can't trust that it was reset correctly */\
    }\
    else {\
    /* case 4: new value is left, old value is right */\
        /* remove old player and don't draw the new one */\
        UBYTE save_graf = GRAFP##n;\
        GRAFP##n = 0;\
        UPDATE_PM_CYCLE_EXACT\
        GRAFP##n = save_graf;\
    }
#else
#define CYCLE_EXACT_HPOSP(n)
#endif /* NEW_CYCLE_EXACT */

#define DO_HPOSP(n) case _HPOSP##n:                             \
    hposp_ptr[n] = pm_scanline + byte - 0x20;                   \
    if (byte >= 0x22) {                                         \
        if (byte > 0xbe) {                                      \
            if (byte >= 0xde)                                   \
                hposp_mask[n] = 0;                              \
            else                                                \
                hposp_mask[n] = 0xffffffff >> (byte - 0xbe);    \
        }                                                       \
        else                                                    \
            hposp_mask[n] = 0xffffffff;                         \
    }                                                           \
    else if (byte > 2)                                          \
        hposp_mask[n] = 0xffffffff << (0x22 - byte);            \
    else                                                        \
        hposp_mask[n] = 0;                                      \
    CYCLE_EXACT_HPOSP(n)\
    HPOSP##n = byte;                                            \
    break;

    DO_HPOSP(0)
    DO_HPOSP(1)
    DO_HPOSP(2)
    DO_HPOSP(3)

/* TODO: cycle-exact size changes */
/* this is only an approximation */
    case _SIZEM:
        SIZEM = byte;
        global_sizem[0] = PM_Width[byte & 0x03];
        global_sizem[1] = PM_Width[(byte & 0x0c) >> 2];
        global_sizem[2] = PM_Width[(byte & 0x30) >> 4];
        global_sizem[3] = PM_Width[(byte & 0xc0) >> 6];
        UPDATE_PM_CYCLE_EXACT
        break;
    case _SIZEP0:
        SIZEP0 = byte;
        grafp_ptr[0] = &grafp_lookup[(byte & 3)*256];
        UPDATE_PM_CYCLE_EXACT
        break;
    case _SIZEP1:
        SIZEP1 = byte;
        grafp_ptr[1] = &grafp_lookup[(byte & 3)*256];
        UPDATE_PM_CYCLE_EXACT
        break;
    case _SIZEP2:
        SIZEP2 = byte;
        grafp_ptr[2] = &grafp_lookup[(byte & 3)*256];
        UPDATE_PM_CYCLE_EXACT
        break;
    case _SIZEP3:
        SIZEP3 = byte;
        grafp_ptr[3] = &grafp_lookup[(byte & 3)*256];
        UPDATE_PM_CYCLE_EXACT
        break;
    case _PRIOR:
#ifdef NEW_CYCLE_EXACT
#ifndef NO_GTIA11_DELAY
        /* update prior change ring buffer */
        prior_curpos = (prior_curpos + 1) % PRIOR_BUF_SIZE;
        prior_pos_buf[prior_curpos] = XPOS * 2 - 37 + 2;
        prior_val_buf[prior_curpos] = byte;
#endif
#endif
        set_prior(byte);
        PRIOR = byte;
        if (byte & 0x40)
            setup_gtia9_11();
        break;
    case _VDELAY:
        VDELAY = byte;
        break;
    case _GRACTL:
        GRACTL = byte;
        missile_gra_enabled = (byte & 0x01);
        player_gra_enabled = (byte & 0x02);
        player_flickering = ((player_dma_enabled | player_gra_enabled) == 0x02);
        missile_flickering = ((missile_dma_enabled | missile_gra_enabled) == 0x01);
        if ((byte & 4) == 0)
            TRIG_latch[0] = TRIG_latch[1] = TRIG_latch[2] = TRIG_latch[3] = 1;
        break;
    }
}


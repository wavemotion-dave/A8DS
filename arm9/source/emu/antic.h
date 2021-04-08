/*
 * ANTIC.C contains the emulation of the ANTIC chip on the Atari 800. 
 * The baseline for this file is the Atari800 4.20 source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 *
 * XEGS-DS - Atari 8-bit Emulator designed to run 8-bit games on the Nintendo DS/DSi
 * Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
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

extern UBYTE CHACTL;
extern UBYTE CHBASE;
extern UWORD dlist;
extern UBYTE DMACTL;
extern UBYTE HSCROL;
extern UBYTE NMIEN;
extern UBYTE NMIST;
extern UBYTE PMBASE;
extern UBYTE VSCROL;

extern int break_ypos;
extern int ypos;
extern UBYTE wsync_halt;

extern unsigned int screenline_cpu_clock;

#define ANTIC_CPU_CLOCK (screenline_cpu_clock + xpos)
#define ANTIC_xpos xpos
#define ANTIC_ypos ypos

#define NMIST_C	6
#define NMI_C	12

#define XPOS xpos

extern int global_artif_mode;

extern UBYTE PENH_input;
extern UBYTE PENV_input;

void ANTIC_Initialise(void);
void ANTIC_Reset(void);
void ANTIC_Frame(int draw_display);
//void ANTIC_Frame(void);
UBYTE ANTIC_GetByte(UWORD addr);
void ANTIC_PutByte(UWORD addr, UBYTE byte);

UBYTE ANTIC_GetDLByte(UWORD *paddr);
UWORD ANTIC_GetDLWord(UWORD *paddr);

/* always call ANTIC_UpdateArtifacting after changing global_artif_mode */
void ANTIC_UpdateArtifacting(void);

/* Video memory access */
void video_memset(UBYTE *ptr, UBYTE val, ULONG size);
void video_putbyte(UBYTE *ptr, UBYTE val);

#endif /* _ANTIC_H_ */

/*
 * CPU.H contains the emulation of the Core 6502 CPU on the Atari 800. 
 * 
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly ecommended that you seek out the latest Atari800 sources.
 *
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
 *
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

#ifndef _CPU_H_
#define _CPU_H_

#include "atari.h"

#define N_FLAG 0x80
#define V_FLAG 0x40
#define B_FLAG 0x10
#define D_FLAG 0x08
#define I_FLAG 0x04
#define Z_FLAG 0x02
#define C_FLAG 0x01

void CPU_Initialise(void);      /* used in the assembler version of cpu.c only */
void CPU_GetStatus(void);
void CPU_PutStatus(void);
void CPU_Reset(void);
void NMI(void);
void GO(int limit);
#define GenerateIRQ() (IRQ = 1)

extern UWORD regPC;
extern UBYTE regA;
extern UBYTE regP;
extern UBYTE regS;
extern UBYTE regY;
extern UBYTE regX;
extern UBYTE N;
extern UBYTE Z;
extern UBYTE C;


#define CPU_regPC regPC
#define CPU_regA  regA
#define CPU_regP  regP
#define CPU_regS  regS
#define CPU_regY  regY
#define CPU_regX  regX

#define SetN regP |= N_FLAG
#define ClrN regP &= (~N_FLAG)
#define SetV regP |= V_FLAG
#define ClrV regP &= (~V_FLAG)
#define SetB regP |= B_FLAG
#define ClrB regP &= (~B_FLAG)
#define SetD regP |= D_FLAG
#define ClrD regP &= (~D_FLAG)
#define SetI regP |= I_FLAG
#define ClrI regP &= (~I_FLAG)
#define SetZ regP |= Z_FLAG
#define ClrZ regP &= (~Z_FLAG)
#define SetC regP |= C_FLAG
#define ClrC regP &= (~C_FLAG)

#define CPU_SetN SetN
#define CPU_ClrN ClrN
#define CPU_SetV SetV
#define CPU_ClrV ClrV
#define CPU_SetZ SetZ
#define CPU_ClrZ ClrZ
#define CPU_SetC SetC
#define CPU_ClrC ClrC

extern UBYTE IRQ;

#define CPU_rts_handler rts_handler

extern void (*rts_handler)(void);

extern UBYTE cim_encountered;

#define CPU_cim_encountered cim_encountered

#endif /* _CPU_H_ */

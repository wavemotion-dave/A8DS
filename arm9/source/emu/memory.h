/*
 * MEMORY.H Various macros to access main and extended memory...
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
 * memory.h - memory emulation
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

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <string.h> /* memcpy, memset */

#include "atari.h"

#define dGetWord(x)                     (dGetByte(x) | (dGetByte((x) + 1) << 8))
#define dPutWord(x, y)                  (dPutByte(x,(UBYTE)y), dPutByte(x+1, (UBYTE) ((y) >> 8)))
#define dGetWordAligned(x)              dGetWord(x)
#define dPutWordAligned(x, y)           dPutWord(x, y)
#define dFillMem(addr1, value, length)  memset(memory + (addr1), value, length)

#define RAM       0
#define ROM       1
#define HARDWARE  2

extern UBYTE *mem_map[16];
extern UBYTE memory[65536 + 2];
extern const UBYTE *antic_xe_ptr;
extern UBYTE ROM_basic[];

// ---------------------------------------------------------------------------------------
// Handles bank switching - we use a memory map so we can easily swap in/out various
// banks of RAM or Cartridge memory. The mem_map[] bank of pointers is always offset
// by the start of each bank so that we don't have to mask addr when indexing. Thanks
// to Phaeron (Altirra) for the keen suggestion!
// ---------------------------------------------------------------------------------------
inline UBYTE dGetByte(UWORD addr)
{
    return *(const UBYTE *)(mem_map[addr >> 12] + addr);
}

inline void dPutByte(UWORD addr, UBYTE data)
{
    *((UBYTE *) mem_map[addr >> 12] + addr) = data;
}

inline UBYTE *AnticMainMemLookup(unsigned int addr)
{
    return (UBYTE *) mem_map[addr >> 12] + addr;
}

inline void dCopyFromMem(unsigned int from, void* to, unsigned int size)
{
    memcpy((UBYTE*)to, AnticMainMemLookup((unsigned int)from), size);
}

inline void dCopyToMem(void*from, unsigned int to, unsigned int size)       
{
    memcpy(AnticMainMemLookup((unsigned int)to), (UBYTE*)from, size);
}

typedef UBYTE (*rdfunc)(UWORD addr);
typedef void (*wrfunc)(UWORD addr, UBYTE value);
extern rdfunc readmap[256];
extern wrfunc writemap[256];
void ROM_PutByte(UWORD addr, UBYTE byte); 

#define GetByte(addr)       (readmap[(addr) >> 8] ? (*readmap[(addr) >> 8])(addr) : dGetByte(addr))
#define PutByte(addr,byte)  (writemap[(addr) >> 8] ? (*writemap[(addr) >> 8])(addr, byte) : (dPutByte(addr, byte)))
#define SetRAM(addr1, addr2) do { \
        int i; \
        for (i = (addr1) >> 8; i <= (addr2) >> 8; i++) { \
            readmap[i] = NULL; \
            writemap[i] = NULL; \
        } \
    } while (0)
#define SetROM(addr1, addr2) do { \
        int i; \
        for (i = (addr1) >> 8; i <= (addr2) >> 8; i++) { \
            readmap[i] = NULL; \
            writemap[i] = ROM_PutByte; \
        } \
    } while (0)

#define MEMORY_GetByte(addr)        (readmap[(addr) >> 8] ? (*readmap[(addr) >> 8])(addr) : dGetByte(addr))
#define MEMORY_PutByte(addr,byte)   (writemap[(addr) >> 8] ? (*writemap[(addr) >> 8])(addr, byte) : (dPutByte(addr, byte)))
#define MEMORY_SetRAM(addr1, addr2) do { \
        int i; \
        for (i = (addr1) >> 8; i <= (addr2) >> 8; i++) { \
            readmap[i] = NULL; \
            writemap[i] = NULL; \
        } \
    } while (0)
#define MEMORY_SetROM(addr1, addr2) do { \
        int i; \
        for (i = (addr1) >> 8; i <= (addr2) >> 8; i++) { \
            readmap[i] = NULL; \
            writemap[i] = ROM_PutByte; \
        } \
    } while (0)


void MEMORY_InitialiseMachine(void);
void CopyFromMem(UWORD from, UBYTE *to, int size);
void CopyToMem(const UBYTE *from, UWORD to, int size);
void MEMORY_HandlePORTB(UBYTE byte, UBYTE oldval);
void Cart809F_Disable(void);
void Cart809F_Enable(void);
void CartA0BF_Disable(void);
void CartA0BF_Enable(void);

#endif /* _MEMORY_H_ */

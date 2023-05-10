/*
 * MEMORY.H Various macros to access main and extended memory...
 *
 * XEGS-DS - Atari 8-bit Emulator designed to run 8-bit games on the Nintendo DS/DSi
 * Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */
#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <string.h>	/* memcpy, memset */

#include "atari.h"

#define dGetWord(x)				        (dGetByte(x) + (dGetByte((x) + 1) << 8))
#define dPutWord(x, y)			        (dPutByte(x,(UBYTE)y), dPutByte(x+1, (UBYTE) ((y) >> 8)))
#define dGetWordAligned(x)		        dGetWord(x)
#define dPutWordAligned(x, y)	        dPutWord(x, y)
#define dFillMem(addr1, value, length)	memset(memory + (addr1), value, length)

#define RAM       0
#define ROM       1
#define HARDWARE  2

extern UBYTE *mem_map[16];
extern UBYTE memory[65536 + 2];

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

#define GetByte(addr)		(readmap[(addr) >> 8] ? (*readmap[(addr) >> 8])(addr) : dGetByte(addr))
#define PutByte(addr,byte)	(writemap[(addr) >> 8] ? (*writemap[(addr) >> 8])(addr, byte) : (dPutByte(addr, byte)))
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

#define MEMORY_GetByte(addr)		(readmap[(addr) >> 8] ? (*readmap[(addr) >> 8])(addr) : dGetByte(addr))
#define MEMORY_PutByte(addr,byte)	(writemap[(addr) >> 8] ? (*writemap[(addr) >> 8])(addr, byte) : (dPutByte(addr, byte)))
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

//#define CopyROM(addr1, addr2, src) memcpy(memory + (addr1), src, (addr2) - (addr1) + 1)

void MEMORY_InitialiseMachine(void);
void CopyFromMem(UWORD from, UBYTE *to, int size);
void CopyToMem(const UBYTE *from, UWORD to, int size);
void MEMORY_HandlePORTB(UBYTE byte, UBYTE oldval);
void Cart809F_Disable(void);
void Cart809F_Enable(void);
void CartA0BF_Disable(void);
void CartA0BF_Enable(void);


#endif /* _MEMORY_H_ */

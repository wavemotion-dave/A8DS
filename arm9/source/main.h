#ifndef _MAIN_H
#define _MAIN_H

#include <nds.h>

#define EMU_INIT       1
#define EMU_END        2

extern int bg0, bg1, bg2, bg3;             // BG pointers 
extern int bg0s, bg1s, bg2s, bg3s;         // sub BG pointers 

extern volatile u16 vusCptVBL;             // VBL test

#endif

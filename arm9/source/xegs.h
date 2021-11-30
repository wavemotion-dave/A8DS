/*
 * XEGS.H contains externs and defines related to XEGS-DS emulator.
 *
 * XEGS-DS - Atari 8-bit Emulator designed to run 8-bit games on the Nintendo DS/DSi
 * Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.  This file is offered as-is,
 * without any warranty.
 */
#ifndef _XEGS_H
#define _XEGS_H

#define A8_MENUINIT 0x01
#define A8_MENUSHOW 0x02
#define A8_PLAYINIT 0x03 
#define A8_PLAYGAME 0x04 
#define A8_QUITSTDS 0x05

extern unsigned short emu_state; 

typedef enum {
  EMUARM7_INIT_SND = 0x123C,
  EMUARM7_STOP_SND = 0x123D,
  EMUARM7_PLAY_SND = 0x123E,
} FifoMesType;

typedef struct FICtoLoad {
  char filename[300];
  bool directory;
  unsigned int uCrc;
} FICA_A8;

extern int bg0, bg1, bg0b,bg1b;
extern int bg2, bg3;
extern unsigned int video_height;                  // Actual video height

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern void vblankIntr();
extern void dsInitScreenMain(void);
extern void dsShowScreenEmu(void);
extern void dsShowScreenMain(void);
extern void dsFreeEmu(void);
extern void VsoundHandler(void);
extern void dsLoadGame(char *filename, int disk_num, bool bRestart, bool bReadOnly);
extern unsigned int dsReadPad(void);
extern bool dsWaitOnQuit(void);
extern void dsDisplayFiles(unsigned int NoDebGame,u32 ucSel);
extern unsigned int dsWaitForRom(void);
extern unsigned int dsWaitOnMenu(unsigned int actState);
extern void dsPrintValue(int x, int y, unsigned int isSelect, char *pchStr);
extern void dsInstallSoundEmuFIFO(void);
extern void dsMainLoop(void);
extern int a8Filescmp (const void *c1, const void *c2);
extern void a8FindFiles(void);

extern void InitGameSettings(void);
extern void WriteGameSettings(void);
extern void ReadGameSettings(void);
extern void ApplyGameSpecificSettings(void);


#endif // _XEGS_H

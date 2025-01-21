/*
 * A8DS.H contains externs and defines related to A8DS emulator.
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
 * be distributed using that same licensing model. See COPYING for the full license.
 */
#ifndef _A8DS_H
#define _A8DS_H

#include "printf.h"

#define A8_MENUINIT 0x01
#define A8_MENUSHOW 0x02
#define A8_PLAYINIT 0x03 
#define A8_PLAYGAME 0x04 
#define A8_QUITSTDS 0x05

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

#define MAX_FILES       1024    // No more than this many files can be processed per directory
#define MAX_FILENAME    299     // Filenames with full path no longer than this

extern unsigned short emu_state; 

typedef enum {
  EMUARM7_INIT_SND = 0x123C,
  EMUARM7_STOP_SND = 0x123D,
  EMUARM7_PLAY_SND = 0x123E,
} FifoMesType;

typedef struct FICtoLoad {
  char filename[MAX_FILENAME];
  u8   directory;
} FICA_A8;

extern int bg0, bg1, bg0b,bg1b;
extern int bg2, bg3;
extern unsigned int video_height;                  // Actual video height
extern u8 bFirstLoad;
extern char last_boot_file[];

extern u16 gTotalAtariFrames;
extern u16 emu_state;
extern u16 atari_frames;

extern u16 sound_idx;
extern u8 myPokeyBufIdx;

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern void vblankIntr();
extern void dsInitScreenMain(void);
extern void dsInitTimer(void);
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
extern void dsShowRomInfo(void);
extern void InitGameSettings(void);
extern void WriteGameSettings(void);
extern void WriteGlobalSettings(void);
extern void ReadGameSettings(void);
extern void ApplyGameSpecificSettings(void);
extern void install_os(void);
extern void dsSetAtariPalette(void);

#endif // _A8DS_H

#ifndef _A5200UTILS_H
#define _A5200UTILS_H

#define A5200_MENUINIT 0x01
#define A5200_MENUSHOW 0x02
#define A5200_PLAYINIT 0x03 
#define A5200_PLAYGAME 0x04 
#define A5200_QUITSTDS 0x05

extern unsigned int etatEmu;

typedef enum {
  EMUARM7_INIT_SND = 0x123C,
  EMUARM7_STOP_SND = 0x123D,
  EMUARM7_PLAY_SND = 0x123E,
} FifoMesType;

typedef struct FICtoLoad {
  char filename[300];
  bool directory;
  unsigned int uCrc;
} FICA5200;

#define ds_GetTicks() (TIMER0_DATA)

extern int bg0, bg1, bg0b,bg1b;
extern unsigned int video_height;                  // Actual video height

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);

extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);

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
extern ITCM_CODE void dsMainLoop(void);
extern int a8Filescmp (const void *c1, const void *c2);
extern void a8FindFiles(void);

#endif

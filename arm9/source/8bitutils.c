#include <nds.h>
#include <nds/fifomessages.h>

#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "main.h"
#include "8bitutils.h"

#include "atari.h"
#include "global.h"
#include "cartridge.h"
#include "input.h"
#include "emu/pia.h"

#include "clickNoQuit_wav.h"
#include "keyclick_wav.h"
#include "bgBottom.h"
#include "bgTop.h"
#include "bgFileSel.h"
#include "bgInfo.h"
#include "bgKeyboard.h"
#include "altirraos_xl.h"

FICA5200 a5200romlist[1024];  
unsigned int counta5200=0, countfiles=0, ucFicAct=0;
int gTotalAtariFrames = 0;
int bg0, bg1, bg0b,bg1b;
unsigned int etatEmu;
gamecfg GameConf;                       // Game Config svg

int myGame_offset_x = 32;
int myGame_offset_y = 20;
int myGame_scale_x = 256;
int myGame_scale_y = 256;


#define  cxBG (myGame_offset_x<<8)
#define  cyBG (myGame_offset_y<<8)
#define  xdxBG (((320 / myGame_scale_x) << 8) | (320 % myGame_scale_x))
#define  ydyBG (((256 / myGame_scale_y) << 8) | (256 % myGame_scale_y))
  
unsigned int atari_pal16[256] = {0};
unsigned char *filebuffer;


signed char sound_buffer[SNDLENGTH];
signed char *psound_buffer;

int alpha_1 = 8;
int alpha_2 = 8;

#define MAX_DEBUG 7
int debug[MAX_DEBUG]={0};
//#define DEBUG_DUMP

static void DumpDebugData(void)
{
#ifdef DEBUG_DUMP
    char dbgbuf[32];
    for (int i=0; i<MAX_DEBUG; i++)
    {
        int idx=0;
        int val = debug[i];
        if (val < 0)
        {
            dbgbuf[idx++] = '-';
            val = val * -1;
        }
        else
        {
            dbgbuf[idx++] = '0' + (int)val/10000000;
        }
        val = val % 10000000;
        dbgbuf[idx++] = '0' + (int)val/1000000;
        val = val % 1000000;
        dbgbuf[idx++] = '0' + (int)val/100000;
        val = val % 100000;
        dbgbuf[idx++] = '0' + (int)val/10000;
        val = val % 10000;
        dbgbuf[idx++] = '0' + (int)val/1000;
        val= val % 1000;
        dbgbuf[idx++] = '0' + (int)val/100;
        val = val % 100;
        dbgbuf[idx++] = '0' + (int)val/10;
        dbgbuf[idx++] = '0' + (int)val%10;
        dbgbuf[idx++] = 0;
        dsPrintValue(0,3+i,0, dbgbuf);
    }
#endif
}

void VsoundHandler(void) 
{
  static unsigned int sound_idx = 0;
  extern unsigned char pokey_buffer[];
  extern int pokeyBufIdx;
  static int myPokeyBufIdx=0;
  static unsigned char lastSample = 0;
  
  // If there is a fresh sample... 
  if (myPokeyBufIdx != pokeyBufIdx)
  {
      lastSample = pokey_buffer[myPokeyBufIdx];
      myPokeyBufIdx = (myPokeyBufIdx+1) & (SNDLENGTH-1);
  }
  sound_buffer[sound_idx] = lastSample;
  sound_idx = (sound_idx+1) & (SNDLENGTH-1);
}

// Color fading effect
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) {
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out vers le noir
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}

#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);
// Table of CRC-32's of all single-byte values (made by make_crc_table)
unsigned int crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

// cal crc32 of a file
unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len) {
  if (buf == 0) return 0;
  crc = crc ^ 0xffffffff;
  while (len >= 8) {
    DO8(buf);
    len -= 8;
  }
  if (len) do {
    DO1(buf);
  } while (--len);
  return crc ^ 0xffffffff;
}

void vblankIntr() 
{
    static int sIndex = 0;
    static const u8 jitter[] = 
    {
        0x00, 0x33, 
        0x88, 0x44
    };

    REG_BG2PA = xdxBG; 
    REG_BG2PD = ydyBG; 

    REG_BG2X = cxBG+jitter[sIndex++]; 
    REG_BG2Y = cyBG+jitter[sIndex++]; 
    sIndex = sIndex & 3;
    
}

void dsInitScreenMain(void) 
{
    // Init vbl and hbl func
    SetYtrigger(190); //trigger 2 lines before vsync
    irqSet(IRQ_VBLANK, vblankIntr);
    irqEnable(IRQ_VBLANK | IRQ_VCOUNT);
    vramSetBankD(VRAM_D_MAIN_BG_0x06040000 ); // Not using this for video but for cartridge bank swap area... it's faster!
    vramSetBankE(VRAM_E_LCD );                // Not using this for video but 64K of faster RAM always useful!  Mapped at 0x06880000
}

void dsInitTimer(void) 
{
    TIMER0_DATA=0;
    TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024; 
}

void dsShowScreenEmu(void) 
{
  // Change vram
  videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
  bg2 = bgInit(2, BgType_Bmp8, BgSize_B8_512x512, 0,0);

  REG_BG2PB = 0;
  REG_BG2PC = 0;

  REG_BG2X = cxBG; 
  REG_BG2Y = cyBG; 
  REG_BG2PA = xdxBG; 
  REG_BG2PD = ydyBG; 
}


void dsShowScreenMain(void) {
  // Init BG mode for 16 bits colors
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
  bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
  bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 30,0);
  bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);

  decompress(bgTopTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(bgTopMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void *) bgTopPal,(u16*) BG_PALETTE,256*2);

  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;
  
  swiWaitForVBlank();
}

void dsFreeEmu(void) {
  // Stop timer of sound
  TIMER2_CR=0; irqDisable(IRQ_TIMER2); 
}

#define PALETTE_SIZE 768
byte palette_data[PALETTE_SIZE] = {
0x00,0x00,0x00,0x25,0x25,0x25,0x34,0x34,0x34,0x4F,0x4F,0x4F,
0x5B,0x5B,0x5B,0x69,0x69,0x69,0x7B,0x7B,0x7B,0x8A,0x8A,0x8A,
0xA7,0xA7,0xA7,0xB9,0xB9,0xB9,0xC5,0xC5,0xC5,0xD0,0xD0,0xD0,
0xD7,0xD7,0xD7,0xE1,0xE1,0xE1,0xF4,0xF4,0xF4,0xFF,0xFF,0xFF,
0x4C,0x32,0x00,0x62,0x3A,0x00,0x7B,0x4A,0x00,0x9A,0x60,0x00,
0xB5,0x74,0x00,0xCC,0x85,0x00,0xE7,0x9E,0x08,0xF7,0xAF,0x10,
0xFF,0xC3,0x18,0xFF,0xD0,0x20,0xFF,0xD8,0x28,0xFF,0xDF,0x30,
0xFF,0xE6,0x3B,0xFF,0xF4,0x40,0xFF,0xFA,0x4B,0xFF,0xFF,0x50,
0x99,0x25,0x00,0xAA,0x25,0x00,0xB4,0x25,0x00,0xD3,0x30,0x00,
0xDD,0x48,0x02,0xE2,0x50,0x09,0xF4,0x67,0x00,0xF4,0x75,0x10,
0xFF,0x9E,0x10,0xFF,0xAC,0x20,0xFF,0xBA,0x3A,0xFF,0xBF,0x50,
0xFF,0xC6,0x6D,0xFF,0xD5,0x80,0xFF,0xE4,0x90,0xFF,0xE6,0x99,
0x98,0x0C,0x0C,0x99,0x0C,0x0C,0xC2,0x13,0x00,0xD3,0x13,0x00,
0xE2,0x35,0x00,0xE3,0x40,0x00,0xE4,0x40,0x20,0xE5,0x52,0x30,
0xFD,0x78,0x54,0xFF,0x8A,0x6A,0xFF,0x98,0x7C,0xFF,0xA4,0x8B,
0xFF,0xB3,0x9E,0xFF,0xC2,0xB2,0xFF,0xD0,0xBA,0xFF,0xD7,0xC0,
0x99,0x00,0x00,0xA9,0x00,0x00,0xC2,0x04,0x00,0xD3,0x04,0x00,
0xDA,0x04,0x00,0xDB,0x08,0x00,0xE4,0x20,0x20,0xF6,0x40,0x40,
0xFB,0x70,0x70,0xFB,0x7E,0x7E,0xFB,0x8F,0x8F,0xFF,0x9F,0x9F,
0xFF,0xAB,0xAB,0xFF,0xB9,0xB9,0xFF,0xC9,0xC9,0xFF,0xCF,0xCF,
0x7E,0x00,0x50,0x80,0x00,0x50,0x80,0x00,0x5F,0x95,0x0B,0x74,
0xAA,0x22,0x88,0xBB,0x2F,0x9A,0xCE,0x3F,0xAD,0xD7,0x5A,0xB6,
0xE4,0x67,0xC3,0xEF,0x72,0xCE,0xFB,0x7E,0xDA,0xFF,0x8D,0xE1,
0xFF,0x9D,0xE5,0xFF,0xA5,0xE7,0xFF,0xAF,0xEA,0xFF,0xB8,0xEC,
0x48,0x00,0x6C,0x5C,0x04,0x88,0x65,0x0D,0x90,0x7B,0x23,0xA7,
0x93,0x3B,0xBF,0x9D,0x45,0xC9,0xA7,0x4F,0xD3,0xB2,0x5A,0xDE,
0xBD,0x65,0xE9,0xC5,0x6D,0xF1,0xCE,0x76,0xFA,0xD5,0x83,0xFF,
0xDA,0x90,0xFF,0xDE,0x9C,0xFF,0xE2,0xA9,0xFF,0xE6,0xB6,0xFF,
0x1B,0x00,0x70,0x22,0x1B,0x8D,0x37,0x30,0xA2,0x48,0x41,0xB3,
0x59,0x52,0xC4,0x63,0x5C,0xCE,0x6F,0x68,0xDA,0x7D,0x76,0xE8,
0x87,0x80,0xF8,0x93,0x8C,0xFF,0x9D,0x97,0xFF,0xA8,0xA3,0xFF,
0xB3,0xAF,0xFF,0xBC,0xB8,0xFF,0xC4,0xC1,0xFF,0xDA,0xD1,0xFF,
0x00,0x0D,0x7F,0x00,0x12,0xA7,0x00,0x18,0xC0,0x0A,0x2B,0xD1,
0x1B,0x4A,0xE3,0x2F,0x58,0xF0,0x37,0x68,0xFF,0x49,0x79,0xFF,
0x5B,0x85,0xFF,0x6D,0x96,0xFF,0x7F,0xA3,0xFF,0x8C,0xAD,0xFF,
0x96,0xB4,0xFF,0xA8,0xC0,0xFF,0xB7,0xCB,0xFF,0xC6,0xD6,0xFF,
0x00,0x29,0x5A,0x00,0x38,0x76,0x00,0x48,0x92,0x00,0x5C,0xAC,
0x00,0x71,0xC6,0x00,0x86,0xD0,0x0A,0x9B,0xDF,0x1A,0xA8,0xEC,
0x2B,0xB6,0xFF,0x3F,0xC2,0xFF,0x45,0xCB,0xFF,0x59,0xD3,0xFF,
0x7F,0xDA,0xFF,0x8F,0xDE,0xFF,0xA0,0xE2,0xFF,0xB0,0xEB,0xFF,
0x00,0x4A,0x00,0x00,0x4C,0x00,0x00,0x6A,0x20,0x50,0x8E,0x79,
0x40,0x99,0x99,0x00,0x9C,0xAA,0x00,0xA1,0xBB,0x01,0xA4,0xCC,
0x03,0xA5,0xD7,0x05,0xDA,0xE2,0x18,0xE5,0xFF,0x34,0xEA,0xFF,
0x49,0xEF,0xFF,0x66,0xF2,0xFF,0x84,0xF4,0xFF,0x9E,0xF9,0xFF,
0x00,0x4A,0x00,0x00,0x5D,0x00,0x00,0x70,0x00,0x00,0x83,0x00,
0x00,0x95,0x00,0x00,0xAB,0x00,0x07,0xBD,0x07,0x0A,0xD0,0x0A,
0x1A,0xD5,0x40,0x5A,0xF1,0x77,0x82,0xEF,0xA7,0x84,0xED,0xD1,
0x89,0xFF,0xED,0x7D,0xFF,0xFF,0x93,0xFF,0xFF,0x9B,0xFF,0xFF,
0x22,0x4A,0x03,0x27,0x53,0x04,0x30,0x64,0x05,0x3C,0x77,0x0C,
0x45,0x8C,0x11,0x5A,0xA5,0x13,0x1B,0xD2,0x09,0x1F,0xDD,0x00,
0x3D,0xCD,0x2D,0x3D,0xCD,0x30,0x58,0xCC,0x40,0x60,0xD3,0x50,
0xA2,0xEC,0x55,0xB3,0xF2,0x4A,0xBB,0xF6,0x5D,0xC4,0xF8,0x70,
0x2E,0x3F,0x0C,0x36,0x4A,0x0F,0x40,0x56,0x15,0x46,0x5F,0x17,
0x57,0x77,0x1A,0x65,0x85,0x1C,0x74,0x93,0x1D,0x8F,0xA5,0x25,
0xAD,0xB7,0x2C,0xBC,0xC7,0x30,0xC9,0xD5,0x33,0xD4,0xE0,0x3B,
0xE0,0xEC,0x42,0xEA,0xF6,0x45,0xF0,0xFD,0x47,0xF4,0xFF,0x6F,
0x55,0x24,0x00,0x5A,0x2C,0x00,0x6C,0x3B,0x00,0x79,0x4B,0x00,
0xB9,0x75,0x00,0xBB,0x85,0x00,0xC1,0xA1,0x20,0xD0,0xB0,0x2F,
0xDE,0xBE,0x3F,0xE6,0xC6,0x45,0xED,0xCD,0x57,0xF5,0xDB,0x62,
0xFB,0xE5,0x69,0xFC,0xEE,0x6F,0xFD,0xF3,0x77,0xFD,0xF3,0x7F,
0x5C,0x27,0x00,0x5C,0x2F,0x00,0x71,0x3B,0x00,0x7B,0x48,0x00,
0xB9,0x68,0x20,0xBB,0x72,0x20,0xC5,0x86,0x29,0xD7,0x96,0x33,
0xE6,0xA4,0x40,0xF4,0xB1,0x4B,0xFD,0xC1,0x58,0xFF,0xCC,0x55,
0xFF,0xD4,0x61,0xFF,0xDD,0x69,0xFF,0xE6,0x79,0xFF,0xEA,0x98
};


int load_os(char *filename ) 
{
  FILE *romfile = fopen(filename, "rb");
  if (romfile == NULL)
  {
     memcpy(atari_os, ROM_altirraos_xl, 0x4000);
  }
  else
  {
    if (machine_type == MACHINE_OSB)
    {
        fread(atari_os, 0x2800, 1, romfile);
    }
    else
    {
        fread(atari_os, 0x4000, 1, romfile);
    }
    fclose(romfile);  
  }
  
 	return 0;
} /* end load_os */

char last_filename[300] = {0};
void dsLoadGame(char *filename, int disk_num, bool bRestart, bool bReadOnly) 
{
  unsigned int index;
  strcpy(last_filename, filename);
  
  // Free buffer if needed
  TIMER2_CR=0; irqDisable(IRQ_TIMER2); 
	if (filebuffer != 0)
		free(filebuffer);
  
    // load card game if ok
    if (Atari800_OpenFile(filename, bRestart, disk_num, bReadOnly) != AFILE_ERROR) 
    {	
      // Initialize the virtual console emulation 
      dsShowScreenEmu();
        
      memset(sound_buffer, 0x00, SNDLENGTH);

      // Init palette
      for(index = 0; index < 256; index++) {
        unsigned short r = palette_data[(index * 3) + 0];
        unsigned short g = palette_data[(index * 3) + 1];
        unsigned short b = palette_data[(index * 3) + 2];
        BG_PALETTE[index] = RGB8(r, g, b);
        atari_pal16[index] = index;
      }

      psound_buffer=sound_buffer;
      TIMER2_DATA = TIMER_FREQ(SOUND_FREQ);                        
      TIMER2_CR = TIMER_DIV_1 | TIMER_IRQ_REQ | TIMER_ENABLE;	     
      irqSet(IRQ_TIMER2, VsoundHandler);                           
    }
}

unsigned int dsReadPad(void) {
	unsigned int keys_pressed, ret_keys_pressed;

	do {
		keys_pressed = keysCurrent();
	} while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);

	do {
		keys_pressed = keysCurrent();
	} while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
	ret_keys_pressed = keys_pressed;

	do {
		keys_pressed = keysCurrent();
	} while ((keys_pressed & (KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);

	return ret_keys_pressed;
}

bool dsWaitOnQuit(void) {
  bool bRet=false, bDone=false;
  unsigned int keys_pressed;
  unsigned int posdeb=0;
  char szName[32];
  
  decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  
  strcpy(szName,"Quit A5200DS ?");
  dsPrintValue(17,2,0,szName);
  sprintf(szName,"%s","A TO CONFIRM, B TO GO BACK");
  dsPrintValue(16-strlen(szName)/2,23,0,szName);
      
  while(!bDone) {
    strcpy(szName,"          YES          ");
    dsPrintValue(5,10+0,(posdeb == 0 ? 1 :  0),szName);
    strcpy(szName,"          NO           ");
    dsPrintValue(5,14+1,(posdeb == 2 ? 1 :  0),szName);
    swiWaitForVBlank();
  
    // Check pad
    keys_pressed=dsReadPad();
    if (keys_pressed & KEY_UP) {
      if (posdeb) posdeb-=2;
    }
    if (keys_pressed & KEY_DOWN) {
      if (posdeb<1) posdeb+=2;
    }
    if (keys_pressed & KEY_A) {
      bRet = (posdeb ? false : true);
      bDone = true;
    }
    if (keys_pressed & KEY_B) {
      bDone = true;
    }
  }

  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);  

  return bRet;
}

bool bLoadReadOnly = true;
bool bLoadAndBoot = true;
void dsDisplayLoadOptions(void)
{
  char tmpBuf[32];
  sprintf(tmpBuf, "[%c]  READ-ONLY", (bLoadReadOnly ? 'X':' '));
  dsPrintValue(14,1,0,tmpBuf);
  sprintf(tmpBuf, "[%c]  BOOT LOAD", (bLoadAndBoot ? 'Y':' '));
  dsPrintValue(14,2,0,tmpBuf);    
}

void dsDisplayFiles(unsigned int NoDebGame,u32 ucSel) 
{
  unsigned int ucBcl,ucGame;
  u8 maxLen;
  char szName[300];
  
  // Display all games if possible
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) (bgGetMapPtr(bg1b)),32*24*2);
    
  dsDisplayLoadOptions();
  countfiles ? sprintf(szName,"%04d/%04d GAMES",(int)(1+ucSel+NoDebGame),countfiles) : sprintf(szName,"%04d/%04d FOLDERS",(int)(1+ucSel+NoDebGame),counta5200);
  dsPrintValue(14,3,0,szName);
    
  dsPrintValue(31,5,0,(char *) (NoDebGame>0 ? "<" : " "));
  dsPrintValue(31,22,0,(char *) (NoDebGame+14<counta5200 ? ">" : " "));
  sprintf(szName,"%s","A TO SELECT A GAME, B TO GO BACK");
  dsPrintValue(16-strlen(szName)/2,23,0,szName);
  for (ucBcl=0;ucBcl<17; ucBcl++) {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < counta5200) {
      char szName2[300];
      maxLen=strlen(a5200romlist[ucGame].filename);
      strcpy(szName,a5200romlist[ucGame].filename);
      if (maxLen>29) szName[29]='\0';
      if (a5200romlist[ucGame].directory) 
      {
        a5200romlist[ucGame].filename[29] = 0;
        sprintf(szName,"[%s]",a5200romlist[ucGame].filename);
        sprintf(szName2,"%-29s",szName);
        dsPrintValue(0,5+ucBcl,(ucSel == ucBcl ? 1 :  0),szName2);
      }
      else 
      {
        sprintf(szName2,"%-29s",strupr(szName));
        dsPrintValue(1,5+ucBcl,(ucSel == ucBcl ? 1 : 0),szName2);
      }
    }
  }
}

unsigned int dsWaitForRom(void) 
{
  bool bDone=false, bRet=false;
  u32 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00,romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage, uLenFic=0,ucFlip=0, ucFlop=0;
  char szName[300];

  decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  
  nbRomPerPage = (counta5200>=17 ? 17 : counta5200);
  uNbRSPage = (counta5200>=5 ? 5 : counta5200);
  if (ucFicAct>counta5200-nbRomPerPage) {
    firstRomDisplay=counta5200-nbRomPerPage;
    romSelected=ucFicAct-counta5200+nbRomPerPage;
  }
  else {
    firstRomDisplay=ucFicAct;
    romSelected=0;
  }
  dsDisplayFiles(firstRomDisplay,romSelected);
  while (!bDone) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        ucFicAct = (ucFicAct>0 ? ucFicAct-1 : counta5200-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=counta5200-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      } 
    }
    else {
      ucHaut = 0;
    }  
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        ucFicAct = (ucFicAct< counta5200-1 ? ucFicAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<counta5200-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      } 
    }
    else {
      ucBas = 0;
    }  
    if ((keysCurrent() & KEY_R) || (keysCurrent() & KEY_RIGHT)) {
      if (!ucSBas) {
        ucFicAct = (ucFicAct< counta5200-nbRomPerPage ? ucFicAct+nbRomPerPage : counta5200-nbRomPerPage);
        if (firstRomDisplay<counta5200-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = counta5200-nbRomPerPage; }
        if (ucFicAct == counta5200-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      } 
    }
    else {
      ucSBas = 0;
    }  
    if ((keysCurrent() & KEY_L) || (keysCurrent() & KEY_LEFT)) {
      if (!ucSHaut) {
        ucFicAct = (ucFicAct> nbRomPerPage ? ucFicAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucFicAct == 0) romSelected = 0;
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      } 
    }
    else {
      ucSHaut = 0;
    }  
		if ( keysCurrent() & KEY_B ) {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }

    if (keysCurrent() & KEY_A) {
      if (!a5200romlist[ucFicAct].directory) {
        bRet=true;
        bDone=true;
      }
      else {
        chdir(a5200romlist[ucFicAct].filename);
        a52FindFiles();
        ucFicAct = 0;
        nbRomPerPage = (counta5200>=16 ? 16 : counta5200);
        uNbRSPage = (counta5200>=5 ? 5 : counta5200);
        if (ucFicAct>counta5200-nbRomPerPage) {
          firstRomDisplay=counta5200-nbRomPerPage;
          romSelected=ucFicAct-counta5200+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucFicAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }
    
    static int last_x_key = 0;
    if (keysCurrent() & KEY_X)
    {
        if (last_x_key != KEY_X)
        {
            bLoadReadOnly = (bLoadReadOnly ? false:true);
            dsDisplayLoadOptions();
            last_x_key = KEY_X;
        }
    } else last_x_key = 0;
      
    static int last_y_key = 0;
    if (keysCurrent() & KEY_Y)
    {
        if (last_y_key != KEY_Y)
        {
            bLoadAndBoot = (bLoadAndBoot ? false:true);
            dsDisplayLoadOptions();
            last_y_key = KEY_Y;
        }
    } else last_y_key = 0;      
      
    // Scroll la selection courante
    if (strlen(a5200romlist[ucFicAct].filename) > 29) {
      ucFlip++;
      if (ucFlip >= 8) {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+29)>strlen(a5200romlist[ucFicAct].filename)) {
          ucFlop++;
          if (ucFlop >= 8) {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,a5200romlist[ucFicAct].filename+uLenFic,29);
        szName[29] = '\0';
        dsPrintValue(1,5+romSelected,1,szName);
      }
    }
    swiWaitForVBlank();
  }
  
  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  
  return bRet;
}

static u16 shift=0;
static u16 ctrl=0;
static u16 sav1 = 0;
static u16 sav2 = 0;
void dsShowKeyboard(void)
{
  decompress(bgKeyboardTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgKeyboardMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgKeyboardPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  swiWaitForVBlank();
  sav1 = *(bgGetMapPtr(bg1b) + 680);
  sav2 = *(bgGetMapPtr(bg1b) + 683);
}

void dsShowHelp(void)
{
  decompress(bgInfoTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgInfoMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgInfoPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  swiWaitForVBlank();
}

void dsRestoreBottomScreen(void)
{
  decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
  swiWaitForVBlank();
}

unsigned int dsWaitOnMenu(unsigned int actState) {
  unsigned int uState=A5200_PLAYINIT;
  unsigned int keys_pressed;
  bool bDone=false, romSel;
  int iTx,iTy;
  
  while (!bDone) {
    // wait for stylus
    keys_pressed = keysCurrent();
    if (keys_pressed & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ((iTx>206) && (iTx<250) && (iTy>110) && (iTy<129))  { // 207,111  -> 249,128   quit
        soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
        bDone=dsWaitOnQuit();
        if (bDone) uState=A5200_QUITSTDS;
      }
       else if ((iTx>70) && (iTx<180) && (iTy>12) && (iTy<80)) {     // cartridge slot (wide range)
        bDone=true; 
        // Find files in current directory and show it 
        a52FindFiles();
        romSel=dsWaitForRom();
        if (romSel) { uState=A5200_PLAYINIT; 
          dsLoadGame(a5200romlist[ucFicAct].filename, 1, bLoadAndBoot, bLoadReadOnly); }
        else { uState=actState; }
      }
    }
    swiWaitForVBlank();
  }
  
  return uState;
}

void dsPrintValue(int x, int y, unsigned int isSelect, char *pchStr)
{
  u16 *pusEcran,*pusMap;
  u16 usCharac;
  char *pTrTxt=pchStr;
  char ch;

  pusEcran=(u16*) (bgGetMapPtr(bg1b))+x+(y<<5);
  pusMap=(u16*) (bgGetMapPtr(bg0b)+(2*isSelect+24)*32);

  while((*pTrTxt)!='\0' )
  {
    ch = *pTrTxt;
    if (ch >= 'a' && ch <= 'z') ch -= 32; // Faster than strcpy/strtoupper
    usCharac=0x0000;
    if ((ch) == '|')
      usCharac=*(pusMap);
    else if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');
    else
      usCharac=*(pusMap+32+(ch)-'@');
    *pusEcran++=usCharac;
    pTrTxt++;
  }
}

int dsHandleKeyboard(int Tx, int Ty)
{
    int keyPress = AKEY_NONE;
    debug[3] = Tx;
    debug[4] = Ty;
    
    if (Ty <= 8) return AKEY_NONE;

    if (Ty > 8 && Ty < 30)       // Top Row 0-9
    {
        if (Tx <  30) keyPress = AKEY_LEFT;
        else if (Tx <  56) keyPress = AKEY_RIGHT;
        else if (Tx <  80) keyPress = AKEY_UP;
        else if (Tx < 104) keyPress = AKEY_DOWN;
        else if (Tx < 130) keyPress = AKEY_QUESTION;
        else if (Tx < 152) keyPress = AKEY_DOLLAR;
        else if (Tx < 179) keyPress = AKEY_EXCLAMATION;
        else if (Tx < 204) keyPress = AKEY_PARENLEFT;
        else if (Tx < 229) keyPress = AKEY_PARENRIGHT;
        else if (Tx < 255) keyPress = AKEY_BACKSPACE;
    }
    else if (Ty < 56)  // Row QWERTY
    {
        if (Tx <  30) keyPress = AKEY_PLUS;
        else if (Tx <  56) keyPress = AKEY_MINUS;
        else if (Tx <  80) keyPress = AKEY_SLASH;
        else if (Tx < 104) keyPress = AKEY_ASTERISK;
        else if (Tx < 130) keyPress = AKEY_EQUAL;
        else if (Tx < 152) keyPress = AKEY_LESS;
        else if (Tx < 179) keyPress = AKEY_GREATER;
        else if (Tx < 204) keyPress = AKEY_BRACKETLEFT;
        else if (Tx < 229) keyPress = AKEY_BRACKETRIGHT;
        else if (Tx < 255) keyPress = AKEY_DBLQUOTE;
    }
    else if (Ty < 84)  // Numbers Row 1-9,0
    {
        if (Tx <  30) keyPress = AKEY_1;
        else if (Tx <  56) keyPress = AKEY_2;
        else if (Tx <  80) keyPress = AKEY_3;
        else if (Tx < 104) keyPress = AKEY_4;
        else if (Tx < 130) keyPress = AKEY_5;
        else if (Tx < 152) keyPress = AKEY_6;
        else if (Tx < 179) keyPress = AKEY_7;
        else if (Tx < 204) keyPress = AKEY_8;
        else if (Tx < 229) keyPress = AKEY_9;
        else if (Tx < 255) keyPress = AKEY_0;
    }
    else if (Ty < 107)  // QWERTY Row
    {
        if (Tx <  30) keyPress = (shift ? AKEY_q : AKEY_Q);
        else if (Tx <  56) keyPress = (shift ? AKEY_w : AKEY_W);
        else if (Tx <  80) keyPress = (shift ? AKEY_e : AKEY_E);
        else if (Tx < 104) keyPress = (shift ? AKEY_r : AKEY_R);
        else if (Tx < 130) keyPress = (shift ? AKEY_t : AKEY_T);
        else if (Tx < 152) keyPress = (shift ? AKEY_y : AKEY_Y);
        else if (Tx < 179) keyPress = (shift ? AKEY_u : AKEY_U);
        else if (Tx < 204) keyPress = (shift ? AKEY_i : AKEY_I);
        else if (Tx < 229) keyPress = (shift ? AKEY_o : AKEY_O);
        else if (Tx < 255) keyPress = (shift ? AKEY_p : AKEY_P);
    }
    else if (Ty < 134)  // Home Row ASDF-JKL;
    {
        if (Tx <  30) keyPress = (shift ? AKEY_a : AKEY_A);
        else if (Tx <  56) keyPress = (shift ? AKEY_s : AKEY_S);
        else if (Tx <  80) keyPress = (shift ? AKEY_d : AKEY_D);
        else if (Tx < 104) keyPress = (shift ? AKEY_f : AKEY_F);
        else if (Tx < 130) keyPress = (shift ? AKEY_g : AKEY_G);
        else if (Tx < 152) keyPress = (shift ? AKEY_h : AKEY_H);
        else if (Tx < 179) keyPress = (shift ? AKEY_j : AKEY_J);
        else if (Tx < 204) keyPress = (shift ? AKEY_k : AKEY_K);
        else if (Tx < 229) keyPress = (shift ? AKEY_l : AKEY_L);
        else if (Tx < 255) keyPress = AKEY_SEMICOLON;
    }
    else if (Ty < 162)  // Bottom Row ZXCV...
    {
        if (Tx <  30) keyPress = (shift ? AKEY_z : AKEY_Z);
        else if (Tx <  56) keyPress = (shift ? AKEY_x : AKEY_X);
        else if (Tx <  80) keyPress = (shift ? AKEY_c : AKEY_C);
        else if (Tx < 104) keyPress = (shift ? AKEY_v : AKEY_V);
        else if (Tx < 130) keyPress = (shift ? AKEY_b : AKEY_B);
        else if (Tx < 152) keyPress = (shift ? AKEY_n : AKEY_N);
        else if (Tx < 179) keyPress = (shift ? AKEY_m : AKEY_M);
        else if (Tx < 204) keyPress = AKEY_COMMA;
        else if (Tx < 229) keyPress = AKEY_FULLSTOP;
        else if (Tx < 255) keyPress = AKEY_COLON;
    }
    else
    {
        if (Tx <  30) keyPress = AKEY_NONE;
        else if (Tx <  56) keyPress = AKEY_NONE;
        else if (Tx <  80) keyPress = AKEY_SHFT;
        else if (Tx < 104) keyPress = AKEY_CTRL;
        else if (Tx < 130) keyPress = AKEY_CTRL_C;
        else if (Tx < 152) keyPress = AKEY_ESCAPE;
        else if (Tx < 179) keyPress = AKEY_SPACE;
        else if (Tx < 204) keyPress = AKEY_SPACE;
        else if (Tx < 229) keyPress = AKEY_RETURN;
        else if (Tx < 255) keyPress = AKEY_RETURN;
    }
    
    if (keyPress == AKEY_SHFT)
    {
        if (shift == 0)
        {
            *(bgGetMapPtr(bg1b)+680) = 60;
            shift = 1;
        }
        else
        {
            *(bgGetMapPtr(bg1b)+680) = sav1;
            shift = 0;
        }
        keyPress = AKEY_NONE;
    }

    if (keyPress == AKEY_CTRL)
    {
        if (ctrl == 0)
        {
            *(bgGetMapPtr(bg1b)+683) = 60;
            ctrl = 1;
        }
        else
        {
            *(bgGetMapPtr(bg1b)+683) = sav2;
            ctrl = 0;
        }
        keyPress = AKEY_NONE;
    }
    else if (ctrl)  
    {
        keyPress |= AKEY_CTRL;
        ctrl=0;
        *(bgGetMapPtr(bg1b)+683) = sav2;
    }
    
    if (keyPress != AKEY_NONE)
    {
        soundPlaySample(keyclick_wav, SoundFormat_16Bit, keyclick_wav_size, 44100, 127, 64, false, 0);
    }
    return keyPress; 
}
    
//---------------------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void) 
{
    FifoMessage msg;
    msg.SoundPlay.data = &sound_buffer;
    msg.SoundPlay.freq = SOUND_FREQ;
    msg.SoundPlay.volume = 127;
    msg.SoundPlay.pan = 64;
    msg.SoundPlay.loop = 1;
    msg.SoundPlay.format = ((1)<<4) | SoundFormat_8Bit;
    msg.SoundPlay.loopPoint = 0;
    msg.SoundPlay.dataSize = SNDLENGTH >> 2;
    msg.type = EMUARM7_PLAY_SND;
    fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);
}

extern u32 trig0, trig1;
extern u32 stick0;
extern u32 stick1;
int full_speed = 0;

ITCM_CODE void dsMainLoop(void) {
  char fpsbuf[32];
  unsigned int keys_pressed,keys_touch=0, romSel=0;
  int iTx,iTy;
  bool showFps=false;
  bool bShowHelp = false;
  bool bShowKeyboard = false;
  
  // Timers are fed with 33.513982 MHz clock.
  // With DIV_1024 the clock is 32,728.5 ticks per sec...
  TIMER0_DATA=0;
  TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;  
    
  myGame_offset_x = 32;
  myGame_offset_y = 20;
  myGame_scale_x = 256;
  myGame_scale_y = 256;
  
  while(etatEmu != A5200_QUITSTDS) 
  {
    switch (etatEmu) 
    {
    
      case A5200_MENUINIT:
        dsShowScreenMain();
        etatEmu = A5200_MENUSHOW;
        break;
        
      case A5200_MENUSHOW:
        etatEmu =  dsWaitOnMenu(A5200_MENUSHOW);
        Atari800_Initialise();
        break;
        
      case A5200_PLAYINIT:
        dsShowScreenEmu();
        irqEnable(IRQ_TIMER2);  
        etatEmu = A5200_PLAYGAME;
        break;
        
      case A5200_PLAYGAME:
        // 65535 = 1 frame
        // 1 frame = 1/50 ou 1/60 (0.02 ou 0.016 
        // 656 -> 50 fps et 546 -> 60 fps
        if (!full_speed)
        {
          while(TIMER0_DATA < 546)
              ;
        }
        TIMER0_CR=0;
        TIMER0_DATA=0;
        TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;

        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= 32728)   // 1000MS (1 sec)
        {
            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
        
            if (showFps) { siprintf(fpsbuf,"%03d",gTotalAtariFrames); dsPrintValue(0,0,0, fpsbuf); } // Show FPS
            gTotalAtariFrames = 0;
            DumpDebugData();
        }
        
        // Execute one frame
        Atari800_Frame(0);

        // Read keys
        keys_pressed=keysCurrent();
        key_consol = CONSOL_NONE;
            
        key_shift = 0;
        key_code = AKEY_NONE;
        trig0 = ((keys_pressed & KEY_A) || (keys_pressed & KEY_B)) ? 0 : 1;
        stick0 = STICK_CENTRE;
        stick1 = STICK_CENTRE;
                  
        if (keys_pressed & KEY_Y) key_consol &= ~CONSOL_OPTION;
        if (keys_pressed & KEY_X) 
        {
            if (keys_pressed & KEY_L) key_code = AKEY_ESCAPE;
            else if (keys_pressed & KEY_R) key_code = AKEY_RETURN;
            else key_code = AKEY_SPACE;
        }
            
        // if touch screen pressed
        if (keys_pressed & KEY_TOUCH) 
        {
          if (!keys_touch) 
          {
            touchPosition touch;
            touchRead(&touch);
            iTx = touch.px;
            iTy = touch.py;
            debug[0] = iTx;
            debug[1] = iTy;
            
            if (bShowHelp || bShowKeyboard)
            {
                  if (bShowKeyboard)
                  {
                      if ((iTy > 165) && (iTx < 50)) // Touch in the lower corner of the screen closes the keyboard...
                      {
                          bShowKeyboard = false;
                          keys_touch = 1;
                          dsRestoreBottomScreen();
                      }
                      else
                      {
                         key_code = dsHandleKeyboard(iTx, iTy);
                      }
                      keys_touch=1;
                  }
                  else
                  {
                      bShowHelp = false;
                      keys_touch = 1;
                      dsRestoreBottomScreen();
                  }
            }
            else
            {
                if ((iTx>240) && (iTx<256) && (iTy>0) && (iTy<22))  { // Full Speed Toggle ... upper corner...
                   if (keys_touch == 0)
                   {
                       full_speed = 1-full_speed; 
                       if (full_speed) dsPrintValue(30,0,0,"FS"); else dsPrintValue(30,0,0,"  ");
                       keys_touch = 1;
                   }
                }
                else if ((iTx>=0) && (iTx<16) && (iTy>0) && (iTy<22))  { // Show FPS
                   if (keys_touch == 0)
                   {
                       showFps= 1-showFps;
                       dsPrintValue(0,0,0, "   "); 
                       keys_touch = 1;
                   }
                }
                else if ((iTx>130) && (iTx<157) && (iTy>122) && (iTy<150))  // START
                { 
                    if (!keys_touch) soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    key_consol &= ~CONSOL_START;
                    keys_touch = 1;
                }
                else if ((iTx>160) && (iTx<185) && (iTy>122) && (iTy<150))  // SELECT
                { 
                    if (!keys_touch) soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    key_consol &= ~CONSOL_SELECT;
                    keys_touch = 1;
                }
                else if ((iTx>190) && (iTx<210) && (iTy>122) && (iTy<150))  // OPTION
                { 
                    if (!keys_touch) soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    key_consol &= ~CONSOL_OPTION;
                    keys_touch = 1;
                }
                else if ((iTx>215) && (iTx<240) && (iTy>122) && (iTy<150))  // RESET (just reloads the game... not sure what else to do here)
                { 
                    if (!keys_touch) soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    keys_touch = 1;
                    Atari800_Initialise();
                    dsLoadGame(last_filename, 1, true, bLoadReadOnly);   // Force Restart 
                    irqEnable(IRQ_TIMER2); 
                    fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
                }
                else if ((iTx>130) && (iTx<157) && (iTy>160) && (iTy<180))  // HELP
                { 
                    if (!keys_touch) soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    dsShowHelp();
                    bShowHelp = true;
                    keys_touch = 1;
                }
                else if ((iTx>10) && (iTx<70) && (iTy>150) && (iTy<190))  // Keyboard
                { 
                    if (!keys_touch) soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    dsShowKeyboard();
                    bShowKeyboard = true;
                    keys_touch = 1;
                }
                else if ((iTx>35) && (iTx<56) && (iTy>89) && (iTy<106))  // POWER
                { 
                  irqDisable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (0) | SOUND_SET_VOLUME);
                  soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                  if (dsWaitOnQuit()) etatEmu=A5200_QUITSTDS;
                  else { irqEnable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME); }
                }
                else if ((iTx>70) && (iTx<180) && (iTy>12) && (iTy<80)) {     // cartridge slot (wide range)
                  irqDisable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (0) | SOUND_SET_VOLUME);
                  // Find files in current directory and show it 
                  keys_touch=1;
                  a52FindFiles();
                  romSel=dsWaitForRom();
                  if (romSel) { etatEmu=A5200_PLAYINIT; dsLoadGame(a5200romlist[ucFicAct].filename, 1, bLoadAndBoot, bLoadReadOnly); }
                  else { irqEnable(IRQ_TIMER2); }
                  fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
                }
             }
          }
        }
        else 
        {
          keys_touch=0;
        }
      
        if (keys_pressed & KEY_UP) stick0 = STICK_FORWARD;
        if (keys_pressed & KEY_LEFT) stick0 = STICK_LEFT;
        if (keys_pressed & KEY_RIGHT) stick0 = STICK_RIGHT;
        if (keys_pressed & KEY_DOWN) stick0 = STICK_BACK;
        if ((keys_pressed & KEY_UP) && (keys_pressed & KEY_LEFT)) stick0 = STICK_UL; 
        if ((keys_pressed & KEY_UP) && (keys_pressed & KEY_RIGHT)) stick0 = STICK_UR;
        if ((keys_pressed & KEY_DOWN) && (keys_pressed & KEY_LEFT)) stick0 = STICK_LL;
        if ((keys_pressed & KEY_DOWN) && (keys_pressed & KEY_RIGHT)) stick0 = STICK_LR;

        if (keys_pressed & KEY_START) key_consol &= ~CONSOL_START;
        if (keys_pressed & KEY_SELECT) key_consol &= ~CONSOL_SELECT;
        if (gTotalAtariFrames & 1)  // Every other frame...
        {
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_UP))   myGame_offset_y++;
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_DOWN)) myGame_offset_y--;
            if ((keys_pressed & KEY_L) && (keys_pressed & KEY_UP))   if (myGame_scale_y <= 256) myGame_scale_y++;
            if ((keys_pressed & KEY_L) && (keys_pressed & KEY_DOWN)) if (myGame_scale_y >= 192) myGame_scale_y--;
        }            
        break;
    }
  }
}

//----------------------------------------------------------------------------------
// Find files (a78 / bin) available
int a52Filescmp (const void *c1, const void *c2) {
  FICA5200 *p1 = (FICA5200 *) c1;
  FICA5200 *p2 = (FICA5200 *) c2;
  
  return strcmp (p1->filename, p2->filename);
}

void a52FindFiles(void) 
{
	struct stat statbuf;
  DIR *pdir;
  struct dirent *pent;
  char filenametmp[300];
  
  counta5200 = countfiles= 0;
  
  pdir = opendir(".");

  if (pdir) {

    while (((pent=readdir(pdir))!=NULL)) 
    {
      stat(pent->d_name,&statbuf);

      strcpy(filenametmp,pent->d_name);
      if(S_ISDIR(statbuf.st_mode)) 
      {
        if (!( (filenametmp[0] == '.') && (strlen(filenametmp) == 1))) {
          a5200romlist[counta5200].directory = true;
          strcpy(a5200romlist[counta5200].filename,filenametmp);
          counta5200++;
        }
      }
      else 
      {
          if ( (strcasecmp(strrchr(filenametmp, '.'), ".xex") == 0) )  {
            a5200romlist[counta5200].directory = false;
            strcpy(a5200romlist[counta5200].filename,filenametmp);
            counta5200++;countfiles++;
          }
          if ( (strcasecmp(strrchr(filenametmp, '.'), ".atr") == 0) )  {
            a5200romlist[counta5200].directory = false;
            strcpy(a5200romlist[counta5200].filename,filenametmp);
            counta5200++;countfiles++;
          }
      }
    }
    closedir(pdir);
  }
  if (counta5200)
    qsort (a5200romlist, counta5200, sizeof (FICA5200), a52Filescmp);
}

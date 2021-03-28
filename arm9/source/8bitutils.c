#include <nds.h>
#include <nds/fifomessages.h>

#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "main.h"
#include "8bitutils.h"

#include "atari.h"
#include "antic.h"
#include "global.h"
#include "cartridge.h"
#include "input.h"
#include "sound.h"
#include "hash.h"
#include "esc.h"
#include "rtime.h"
#include "emu/pia.h"

#include "clickNoQuit_wav.h"
#include "keyclick_wav.h"
#include "bgBottom.h"
#include "bgTop.h"
#include "bgFileSel.h"
#include "bgInfo.h"
#include "bgKeyboard.h"
#include "bgKeyBrief.h"
#include "altirraos_xl.h"
#include "altirra_basic.h"

FICA_A8 a8romlist[1024];  
unsigned int count8bit=0, countfiles=0, ucFicAct=0;
int gTotalAtariFrames = 0;
int bg0, bg1, bg2, bg3, bg0b, bg1b;
unsigned int etatEmu;
int atari_frames = 0;
extern int global_artif_mode;

short int myGame_offset_x = 32;
short int myGame_offset_y = 24;
short int myGame_scale_x = 256;
short int myGame_scale_y = 256;

int bAtariOS=false;
int bAtariOSB=false;
int bAtariBASIC=false;

extern u8 trig0, trig1;
extern u8 stick0, stick1;
extern int skip_frames;
int full_speed = 0;
int os_type = OS_ALTIRRA_XL;
int basic_type = BASIC_ALTIRRA;
int bHaveBASIC = false;
int bUseA_KeyAsUP = false;
int bUseB_KeyAsDN = false;
int bUseX_KeyAsCR = false;
int key_click_disable = false;
int bShowEmuText=true;
int showFps=false;
int palett_type = 0;
int auto_fire=0;
int ram_type=0;             // default is 128k
int jitter_type = 0;        // Normal... 1=SHARP
int keyboard_type=0;

#define  cxBG (myGame_offset_x<<8)
#define  cyBG (myGame_offset_y<<8)
#define  xdxBG (((320 / myGame_scale_x) << 8) | (320 % myGame_scale_x))
#define  ydyBG (((256 / myGame_scale_y) << 8) | (256 % myGame_scale_y))
#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

signed char sound_buffer[SNDLENGTH];
signed char *psound_buffer;

#define MAX_DEBUG 8
int debug[MAX_DEBUG]={0};
//#define DEBUG_DUMP

bool bAtariCrash = false;

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


char last_filename[300] = {0};
void dsWriteFavs(void)
{
#if 0
    dsPrintValue(4,0,0, (char*)"FAV");
    FILE *fp;
    fp = fopen("/roms/A800-Favs.txt", "a+");
    if (fp != NULL)
    {
        fprintf(fp, "%s\n", last_filename);
        fflush(fp);
        fclose(fp);
    }
#else 
    dsPrintValue(4,0,0, (char*)"CFG");
    WriteGameSettings();
#endif    
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    dsPrintValue(4,0,0, (char*)"   ");
}

void dsClearDiskActivity(void)
{
    char buf[5];
    
    buf[0] = ' ';
    buf[1] = ' ';
    buf[2] = ' ';
    buf[3] = 0;
    dsPrintValue(3,0,0, buf);
}

void dsShowDiskActivity(int drive)
{
    static char activity[7] = {'*','+','*','*','+','+','+'};
    char buf[5];
    static u8 actidx=0;
    
    buf[0] = 'D';
    buf[1] = '1'+drive;
    buf[2] = activity[++actidx & 0x7];
    buf[3] = 0;
    dsPrintValue(3,0,0, buf);
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

#define PALETTE_SIZE 768
byte palette_A[PALETTE_SIZE] = {
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

byte palette_B[PALETTE_SIZE] = {
  0x00, 0x00, 0x00, 0x1c, 0x1c, 0x1c, 0x39, 0x39, 0x39, 0x59, 0x59, 0x59, 
  0x79, 0x79, 0x79, 0x92, 0x92, 0x92, 0xab, 0xab, 0xab, 0xbc, 0xbc, 0xbc, 
  0xcd, 0xcd, 0xcd, 0xd9, 0xd9, 0xd9, 0xe6, 0xe6, 0xe6, 0xec, 0xec, 0xec, 
  0xf2, 0xf2, 0xf2, 0xf8, 0xf8, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0x39, 0x17, 0x01, 0x5e, 0x23, 0x04, 0x83, 0x30, 0x08, 0xa5, 0x47, 0x16, 
  0xc8, 0x5f, 0x24, 0xe3, 0x78, 0x20, 0xff, 0x91, 0x1d, 0xff, 0xab, 0x1d, 
  0xff, 0xc5, 0x1d, 0xff, 0xce, 0x34, 0xff, 0xd8, 0x4c, 0xff, 0xe6, 0x51, 
  0xff, 0xf4, 0x56, 0xff, 0xf9, 0x77, 0xff, 0xff, 0x98, 0xff, 0xff, 0x98, 
  0x45, 0x19, 0x04, 0x72, 0x1e, 0x11, 0x9f, 0x24, 0x1e, 0xb3, 0x3a, 0x20, 
  0xc8, 0x51, 0x22, 0xe3, 0x69, 0x20, 0xff, 0x81, 0x1e, 0xff, 0x8c, 0x25, 
  0xff, 0x98, 0x2c, 0xff, 0xae, 0x38, 0xff, 0xc5, 0x45, 0xff, 0xc5, 0x59, 
  0xff, 0xc6, 0x6d, 0xff, 0xd5, 0x87, 0xff, 0xe4, 0xa1, 0xff, 0xe4, 0xa1, 
  0x4a, 0x17, 0x04, 0x7e, 0x1a, 0x0d, 0xb2, 0x1d, 0x17, 0xc8, 0x21, 0x19, 
  0xdf, 0x25, 0x1c, 0xec, 0x3b, 0x38, 0xfa, 0x52, 0x55, 0xfc, 0x61, 0x61, 
  0xff, 0x70, 0x6e, 0xff, 0x7f, 0x7e, 0xff, 0x8f, 0x8f, 0xff, 0x9d, 0x9e, 
  0xff, 0xab, 0xad, 0xff, 0xb9, 0xbd, 0xff, 0xc7, 0xce, 0xff, 0xc7, 0xce, 
  0x05, 0x05, 0x68, 0x3b, 0x13, 0x6d, 0x71, 0x22, 0x72, 0x8b, 0x2a, 0x8c, 
  0xa5, 0x32, 0xa6, 0xb9, 0x38, 0xba, 0xcd, 0x3e, 0xcf, 0xdb, 0x47, 0xdd, 
  0xea, 0x51, 0xeb, 0xf4, 0x5f, 0xf5, 0xfe, 0x6d, 0xff, 0xfe, 0x7a, 0xfd, 
  0xff, 0x87, 0xfb, 0xff, 0x95, 0xfd, 0xff, 0xa4, 0xff, 0xff, 0xa4, 0xff, 
  0x28, 0x04, 0x79, 0x40, 0x09, 0x84, 0x59, 0x0f, 0x90, 0x70, 0x24, 0x9d, 
  0x88, 0x39, 0xaa, 0xa4, 0x41, 0xc3, 0xc0, 0x4a, 0xdc, 0xd0, 0x54, 0xed, 
  0xe0, 0x5e, 0xff, 0xe9, 0x6d, 0xff, 0xf2, 0x7c, 0xff, 0xf8, 0x8a, 0xff, 
  0xff, 0x98, 0xff, 0xfe, 0xa1, 0xff, 0xfe, 0xab, 0xff, 0xfe, 0xab, 0xff, 
  0x35, 0x08, 0x8a, 0x42, 0x0a, 0xad, 0x50, 0x0c, 0xd0, 0x64, 0x28, 0xd0, 
  0x79, 0x45, 0xd0, 0x8d, 0x4b, 0xd4, 0xa2, 0x51, 0xd9, 0xb0, 0x58, 0xec, 
  0xbe, 0x60, 0xff, 0xc5, 0x6b, 0xff, 0xcc, 0x77, 0xff, 0xd1, 0x83, 0xff, 
  0xd7, 0x90, 0xff, 0xdb, 0x9d, 0xff, 0xdf, 0xaa, 0xff, 0xdf, 0xaa, 0xff, 
  0x05, 0x1e, 0x81, 0x06, 0x26, 0xa5, 0x08, 0x2f, 0xca, 0x26, 0x3d, 0xd4, 
  0x44, 0x4c, 0xde, 0x4f, 0x5a, 0xee, 0x5a, 0x68, 0xff, 0x65, 0x75, 0xff, 
  0x71, 0x83, 0xff, 0x80, 0x91, 0xff, 0x90, 0xa0, 0xff, 0x97, 0xa9, 0xff, 
  0x9f, 0xb2, 0xff, 0xaf, 0xbe, 0xff, 0xc0, 0xcb, 0xff, 0xc0, 0xcb, 0xff, 
  0x0c, 0x04, 0x8b, 0x22, 0x18, 0xa0, 0x38, 0x2d, 0xb5, 0x48, 0x3e, 0xc7, 
  0x58, 0x4f, 0xda, 0x61, 0x59, 0xec, 0x6b, 0x64, 0xff, 0x7a, 0x74, 0xff, 
  0x8a, 0x84, 0xff, 0x91, 0x8e, 0xff, 0x99, 0x98, 0xff, 0xa5, 0xa3, 0xff, 
  0xb1, 0xae, 0xff, 0xb8, 0xb8, 0xff, 0xc0, 0xc2, 0xff, 0xc0, 0xc2, 0xff, 
  0x1d, 0x29, 0x5a, 0x1d, 0x38, 0x76, 0x1d, 0x48, 0x92, 0x1c, 0x5c, 0xac, 
  0x1c, 0x71, 0xc6, 0x32, 0x86, 0xcf, 0x48, 0x9b, 0xd9, 0x4e, 0xa8, 0xec, 
  0x55, 0xb6, 0xff, 0x70, 0xc7, 0xff, 0x8c, 0xd8, 0xff, 0x93, 0xdb, 0xff, 
  0x9b, 0xdf, 0xff, 0xaf, 0xe4, 0xff, 0xc3, 0xe9, 0xff, 0xc3, 0xe9, 0xff, 
  0x2f, 0x43, 0x02, 0x39, 0x52, 0x02, 0x44, 0x61, 0x03, 0x41, 0x7a, 0x12, 
  0x3e, 0x94, 0x21, 0x4a, 0x9f, 0x2e, 0x57, 0xab, 0x3b, 0x5c, 0xbd, 0x55, 
  0x61, 0xd0, 0x70, 0x69, 0xe2, 0x7a, 0x72, 0xf5, 0x84, 0x7c, 0xfa, 0x8d, 
  0x87, 0xff, 0x97, 0x9a, 0xff, 0xa6, 0xad, 0xff, 0xb6, 0xad, 0xff, 0xb6, 
  0x0a, 0x41, 0x08, 0x0d, 0x54, 0x0a, 0x10, 0x68, 0x0d, 0x13, 0x7d, 0x0f, 
  0x16, 0x92, 0x12, 0x19, 0xa5, 0x14, 0x1c, 0xb9, 0x17, 0x1e, 0xc9, 0x19, 
  0x21, 0xd9, 0x1b, 0x47, 0xe4, 0x2d, 0x6e, 0xf0, 0x40, 0x78, 0xf7, 0x4d, 
  0x83, 0xff, 0x5b, 0x9a, 0xff, 0x7a, 0xb2, 0xff, 0x9a, 0xb2, 0xff, 0x9a, 
  0x04, 0x41, 0x0b, 0x05, 0x53, 0x0e, 0x06, 0x66, 0x11, 0x07, 0x77, 0x14, 
  0x08, 0x88, 0x17, 0x09, 0x9b, 0x1a, 0x0b, 0xaf, 0x1d, 0x48, 0xc4, 0x1f, 
  0x86, 0xd9, 0x22, 0x8f, 0xe9, 0x24, 0x99, 0xf9, 0x27, 0xa8, 0xfc, 0x41, 
  0xb7, 0xff, 0x5b, 0xc9, 0xff, 0x6e, 0xdc, 0xff, 0x81, 0xdc, 0xff, 0x81, 
  0x02, 0x35, 0x0f, 0x07, 0x3f, 0x15, 0x0c, 0x4a, 0x1c, 0x2d, 0x5f, 0x1e, 
  0x4f, 0x74, 0x20, 0x59, 0x83, 0x24, 0x64, 0x92, 0x28, 0x82, 0xa1, 0x2e, 
  0xa1, 0xb0, 0x34, 0xa9, 0xc1, 0x3a, 0xb2, 0xd2, 0x41, 0xc4, 0xd9, 0x45, 
  0xd6, 0xe1, 0x49, 0xe4, 0xf0, 0x4e, 0xf2, 0xff, 0x53, 0xf2, 0xff, 0x53, 
  0x26, 0x30, 0x01, 0x24, 0x38, 0x03, 0x23, 0x40, 0x05, 0x51, 0x54, 0x1b, 
  0x80, 0x69, 0x31, 0x97, 0x81, 0x35, 0xaf, 0x99, 0x3a, 0xc2, 0xa7, 0x3e, 
  0xd5, 0xb5, 0x43, 0xdb, 0xc0, 0x3d, 0xe1, 0xcb, 0x38, 0xe2, 0xd8, 0x36, 
  0xe3, 0xe5, 0x34, 0xef, 0xf2, 0x58, 0xfb, 0xff, 0x7d, 0xfb, 0xff, 0x7d, 
  0x40, 0x1a, 0x02, 0x58, 0x1f, 0x05, 0x70, 0x24, 0x08, 0x8d, 0x3a, 0x13, 
  0xab, 0x51, 0x1f, 0xb5, 0x64, 0x27, 0xbf, 0x77, 0x30, 0xd0, 0x85, 0x3a, 
  0xe1, 0x93, 0x44, 0xed, 0xa0, 0x4e, 0xf9, 0xad, 0x58, 0xfc, 0xb7, 0x5c, 
  0xff, 0xc1, 0x60, 0xff, 0xc6, 0x71, 0xff, 0xcb, 0x83, 0xff, 0xcb, 0x83
};

void dsSetAtariPalette(void)
{
    unsigned int index;
    unsigned short r;
    unsigned short g;
    unsigned short b;    
    
    // Init palette
    for(index = 0; index < 256; index++) 
    {
      if (palett_type == 0)
      {
        r = palette_A[(index * 3) + 0];
        g = palette_A[(index * 3) + 1];
        b = palette_A[(index * 3) + 2];
      }
      else
      {
        r = palette_B[(index * 3) + 0];
        g = palette_B[(index * 3) + 1];
        b = palette_B[(index * 3) + 2];
      }
      BG_PALETTE[index] = RGB8(r, g, b);
    }
}

// Color fading effect
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) 
{
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

static int sIndex __attribute__((section(".dtcm")))= 0;
static u8 jitter[] __attribute__((section(".dtcm"))) = 
{
    0x00, 0x00, 
    0x88, 0x11,
    0x44, 0x22,
    0xAA, 0x33, 
};
static u8 jitter_sharp[] __attribute__((section(".dtcm"))) = 
{
    0x11, 0x11, 
    0x33, 0x33,
    0x22, 0x22,
    0x44, 0x44, 
};
void vblankIntr() 
{
    REG_BG2PA = xdxBG; 
    REG_BG2PD = ydyBG; 
    
    REG_BG3PA = xdxBG; 
    REG_BG3PD = ydyBG; 

    if (jitter_type == 0)
    {        
        REG_BG2X = cxBG+jitter[sIndex++]; 
        REG_BG2Y = cyBG+jitter[sIndex++]; 
        REG_BG3X = cxBG+jitter[sIndex++]; 
        REG_BG3Y = cyBG+jitter[sIndex++]; 
    }
    else
    {
        REG_BG2X = cxBG+jitter_sharp[sIndex++]; 
        REG_BG2Y = cyBG+jitter_sharp[sIndex++]; 
        REG_BG3X = cxBG+jitter_sharp[sIndex++]; 
        REG_BG3Y = cyBG+jitter_sharp[sIndex++]; 
    }
    sIndex = sIndex & 7;    
}

/*
A: ARM9 0x06800000 - 0x0681FFFF (128KB)
B: ARM9 0x06820000 - 0x0683FFFF (128KB)
C: ARM9 0x06840000 - 0x0685FFFF (128KB)
D: ARM9 0x06860000 - 0x0687FFFF (128KB)
E: ARM9 0x06880000 - 0x0688FFFF ( 64KB)
F: ARM9 0x06890000 - 0x06893FFF ( 16KB)
G: ARM9 0x06894000 - 0x06897FFF ( 16KB)
H: ARM9 0x06898000 - 0x0689FFFF ( 32KB)
I: ARM9 0x068A0000 - 0x068A3FFF ( 16KB)
*/

void dsInitScreenMain(void) 
{
    // Init vbl and hbl func
    SetYtrigger(190); //trigger 2 lines before vsync
    irqSet(IRQ_VBLANK, vblankIntr);
    irqEnable(IRQ_VBLANK);
    vramSetBankA(VRAM_A_MAIN_BG);             // This is the main Emulation screen - Background 1 (we ALPHA blend this with BG2)
    vramSetBankB(VRAM_B_MAIN_BG);             // This is the main Emulation screen - Background 2 (we ALPHA blend this with BG1)
    vramSetBankC(VRAM_C_SUB_BG);              // This is the Sub-Screen (touch screen) display (2 layers)
    vramSetBankD(VRAM_D_LCD );                // Not using this for video but 128K of faster RAM always useful!  Mapped at 0x06860000
    vramSetBankE(VRAM_E_LCD );                // Not using this for video but  64K of faster RAM always useful!  Mapped at 0x06880000
    vramSetBankF(VRAM_F_LCD );                // Not using this for video but  16K of faster RAM always useful!  Mapped at 0x06890000
    vramSetBankG(VRAM_G_LCD );                // Not using this for video but  16K of faster RAM always useful!  Mapped at 0x06894000
    vramSetBankH(VRAM_H_LCD );                // Not using this for video but  32K of faster RAM always useful!  Mapped at 0x06898000
    vramSetBankI(VRAM_I_LCD );                // Not using this for video but  16K of faster RAM always useful!  Mapped at 0x068A0000
    
    ReadGameSettings();
}

void dsInitTimer(void) 
{
    TIMER0_DATA=0;
    TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024; 
}

void dsShowScreenEmu(void) 
{
  // Change vram
  videoSetMode(MODE_5_2D | DISPLAY_BG2_ACTIVE | DISPLAY_BG3_ACTIVE);
  bg2 = bgInit(2, BgType_Bmp8, BgSize_B8_512x512, 0,0);
  bg3 = bgInit(3, BgType_Bmp8, BgSize_B8_512x512, 0,0);
    
  REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG2 | BLEND_DST_BG3;
  REG_BLDALPHA = (10 << 8) | 10; // 62% / 62% 
    
  REG_BG2PB = 0;
  REG_BG2PC = 0;

  REG_BG2X = cxBG; 
  REG_BG2Y = cyBG; 
  REG_BG2PA = xdxBG; 
  REG_BG2PD = ydyBG; 
    
  REG_BG3PB = 0;
  REG_BG3PC = 0;

  REG_BG3X = cxBG; 
  REG_BG3Y = cyBG; 
  REG_BG3PA = xdxBG; 
  REG_BG3PD = ydyBG;     
}


void dsShowScreenMain(void) 
{
  // Init BG mode for 16 bits colors
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE);
  
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

UBYTE ROM_atarios_xl[0x4000];
UBYTE ROM_atarios_b[0x2800];
UBYTE ROM_basic[0x2000];

void load_os(void) 
{
    FILE *romfile = fopen("atarixl.rom", "rb");
    if (romfile == NULL)
    {
        // If we can't find the atari OS, we force the Altirra XL bios in...
        memcpy(ROM_atarios_xl, ROM_altirraos_xl, 0x4000);
        bAtariOS = false;
    }
    else
    {        
        fread(ROM_atarios_xl, 0x4000, 1, romfile);
        fclose(romfile); 
        bAtariOS = true;
    }
    
    
    romfile = fopen("atariosb.rom", "rb");
    if (romfile == NULL)
    {
        // If we can't find the atari OSB, we force the Altirra 800 bios in
        memcpy(ROM_atarios_b, ROM_altirraos_800, 0x2800);
        bAtariOSB = false;
    }
    else
    {        
        fread(ROM_atarios_b, 0x2800, 1, romfile);
        fclose(romfile); 
        bAtariOSB = true;
    }

    
    FILE *basfile = fopen("ataribas.rom", "rb");
    if (basfile == NULL)
    {
        // If we can't find the atari Basic, we force the Altirra in...
        memcpy(ROM_basic, ROM_altirra_basic, 0x2000);
        bAtariBASIC = false;
    }
    else
    {        
        fread(ROM_basic, 0x2000, 1, basfile);
        fclose(basfile);  
        bAtariBASIC = true;
    }

    return;
} /* end load_os() */

void install_os(void)
{
    // Otherwise we either use the Atari OS or the Altirra based on user choice...
    if (os_type == OS_ALTIRRA_XL)
    {
        memcpy(atari_os, ROM_altirraos_xl, 0x4000);
        machine_type = MACHINE_XLXE;
    }
    else if (os_type == OS_ALTIRRA_800)
    {
        memcpy(atari_os, ROM_altirraos_800, 0x2800);
        machine_type = MACHINE_OSB;
    }
    else if (os_type == OS_ATARI_OSB)
    {
        memcpy(atari_os, ROM_atarios_b, 0x2800);
        machine_type = MACHINE_OSB;
    }
    else // Must be OS_ATARI_XL
    {
        memcpy(atari_os, ROM_atarios_xl, 0x4000);
        machine_type = MACHINE_XLXE;
    }
}

void dsShowRomInfo(void)
{
    extern char disk_filename[DISK_MAX][256];
    char line1[25];
    char ramSizeBuf[8];
    char machineBuf[20];
    char line2[200];
    
    if (bShowEmuText)
    {
        dsPrintValue(10,2,0, "XEX:  ");
        strncpy(line1, disk_filename[DISK_XEX], 22);
        line1[22] = 0;    
        sprintf(line2,"%-22s", line1);
        dsPrintValue(10,3,0, line2);

        sprintf(line1, "D1: %s", (disk_readonly[DISK_1] ? "[R]":"[W]"));
        dsPrintValue(10,6,0, line1);
        strncpy(line1, disk_filename[DISK_1], 22);
        line1[22] = 0;
        sprintf(line2,"%-22s", line1);
        dsPrintValue(10,7,0, line2);
        if (strlen(disk_filename[DISK_1]) > 26)
        {
            strncpy(line1, &disk_filename[DISK_1][22], 22);
            line1[22] = 0;
            sprintf(line2,"%-22s", line1);
        }
        else
        {
            sprintf(line2,"%-22s", " ");
        }
        dsPrintValue(10,8,0, line2);

        sprintf(line1, "D2: %s", (disk_readonly[DISK_2] ? "[R]":"[W]"));
        dsPrintValue(10,11,0, line1);
        strncpy(line1, disk_filename[DISK_2], 22);
        line1[22] = 0;
        sprintf(line2,"%-22s", line1);
        dsPrintValue(10,12,0, line2);
        if (strlen(disk_filename[DISK_2]) > 26)
        {
            strncpy(line1, &disk_filename[DISK_2][22], 22);
            line1[22] = 0;
            sprintf(line2,"%-22s", line1);
        }
        else
        {
            sprintf(line2,"%-22s", " ");
        }
        dsPrintValue(10,13,0, line2);

        sprintf(ramSizeBuf, "%dK", ram_size);
        if ((os_type == OS_ATARI_OSB) || (os_type==OS_ALTIRRA_800))
            sprintf(machineBuf, "%-5s A800", (bHaveBASIC ? "BASIC": " "));
        else
            sprintf(machineBuf, "%-5s XL/XE", (bHaveBASIC ? "BASIC": " "));
        sprintf(line2, "%-12s %-4s %-4s", machineBuf, ramSizeBuf, (tv_mode == TV_NTSC ? "NTSC":"PAL "));
        dsPrintValue(7,0,0, line2);
    }
}

#define HASH_FILE_LEN  (128*1024)
unsigned char tempFileBuf[HASH_FILE_LEN];
unsigned char last_hash[33] = {'1','2','3','4','5','Z',0};
void dsLoadGame(char *filename, int disk_num, bool bRestart, bool bReadOnly) 
{
    // Free buffer if needed
    TIMER2_CR=0; irqDisable(IRQ_TIMER2); 
    
    if (disk_num == DISK_XEX)   // Force restart on XEX load...
    {
        bRestart = true;
    }
    
    if (bRestart) // Only save last filename and hash if we are restarting...
    {
        if (strcmp(filename, last_filename) != 0)
        {
             strcpy(last_filename, filename);
        }
    
        // Get the hash of the file... up to 128k (good enough)
        memset(last_hash, 'Z', 33);
        FILE *fp = fopen(filename, "rb");
        if (fp)
        {   
            unsigned int file_len = fread(tempFileBuf, 1, HASH_FILE_LEN, fp);
            hash_Compute((const byte*)tempFileBuf, file_len, (byte *)last_hash);
            fclose(fp);           
        }
    
        // -------------------------------------------------------------------
        // If we are cold starting, go see if we have settings we can read
        // in from a config file or else set some reasonable defaults ...
        // -------------------------------------------------------------------
        ApplyGameSpecificSettings();
        Atari800_Initialise();   
    }
    
      // load game if ok
    if (Atari800_OpenFile(filename, bRestart, disk_num, bReadOnly, bHaveBASIC) != AFILE_ERROR) 
    {	
      // Initialize the virtual console emulation 
      dsShowScreenEmu();
      
      bAtariCrash = false;
      dsPrintValue(1,23,0, "                              ");
        
      memset(sound_buffer, 0x00, SNDLENGTH);
        
      dsSetAtariPalette();

      // In case we switched PAL/NTSC
      dsInstallSoundEmuFIFO();
      psound_buffer=sound_buffer;
      TIMER2_DATA = TIMER_FREQ(SOUND_FREQ);
      TIMER2_CR = TIMER_DIV_1 | TIMER_IRQ_REQ | TIMER_ENABLE;	     
      irqSet(IRQ_TIMER2, VsoundHandler);        
        
      atari_frames = 0;
      TIMER0_CR=0;
      TIMER0_DATA=0;
      TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;        

      dsShowRomInfo();
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
  
  strcpy(szName,"Quit XEGS-DS?");
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
  dsShowRomInfo();

  return bRet;
}

struct options_t
{
    char *label;
    char *option[5];
    int  *option_val;
    int   option_max;
    char *help1;
    char *help2;
    char *help3;
    char *help4;
};

static int basic_opt=0;
static int tv_type2=0;
const struct options_t Option_Table[] =
{
    {"TV TYPE",     {"NTSC",        "PAL"},                             &tv_type2,              2,          "NTSC=60 FPS       ",   "WITH 262 SCANLINES",  "PAL=50 FPS        ",  "WITH 312 SCANLINES"},
    {"MACHINE TYPE",{"XL/XE 128K",  "XE 320K RAMBO", "ATARI 800 48K"},  &ram_type,              3,          "XL/XE 128K FOR    ",   "MOST GAMES. 320K  ",  "FOR A FEW AND A800",  "FOR COMPATIBILITY "},
    {"OS TYPE",     {"ALTIRRA XL",  "ATARIXL.ROM",     
                     "ALTIRRA 800",  "ATARIOSB.ROM"},                   &os_type,               4,          "BUILT-IN ALTIRRA  ",   "USUALLY. FEW GAMES",  "REQUIRE ATARIXL OR",  "ATARIOSB TO WORK  "},
    {"BASIC",       {"DISABLED",    "ALTIRRA",      "ATARIBAS.ROM"},    &basic_opt,             3,          "NORMALLY DISABLED ",   "EXCEPT FOR BASIC  ",  "GAMES THAT REQUIRE",  "THE CART INSERTED "},
    {"SKIP FRAMES", {"NO",          "MODERATE",     "AGGRESSIVE"},      &skip_frames,           3,          "OFF NORMALLY AS   ",   "SOME GAMES CAN    ",  "GLITCH WHEN SET   ",  "TO FRAMESKIP      "},
    {"PALETTE",     {"BRIGHT",      "MUTED"},                           &palett_type,           2,          "CHOOSE PALLETTE   ",   "THAT BEST SUITS   ",  "YOUR VIEWING      ",  "PREFERENCE        "},
    {"A BUTTON",    {"FIRE",        "UP"},                              &bUseA_KeyAsUP,         2,          "TOGGLE THE A KEY  ",   "BEHAVIOR SUCH THAT",  "IT CAN BE A FIRE  ",  "BUTTON OR JOY UP  "},
    {"B BUTTON",    {"FIRE",        "DOWN"},                            &bUseB_KeyAsDN,         2,          "TOGGLE THE B KEY  ",   "BEHAVIOR SUCH THAT",  "IT CAN BE A FIRE  ",  "BUTTON OR JOY DOWN"},
    {"X BUTTON",    {"SPACE",       "RETURN"},                          &bUseX_KeyAsCR,         2,          "TOGGLE THE X KEY  ",   "BEHAVIOR SUCH THAT",  "IT CAN BE SPACE OR",  "RETURN KEY        "},
    {"AUTOFIRE",    {"OFF",         "SLOW",   "MED",  "FAST"},          &auto_fire,             4,          "TOGGLE AUTOFIRE   ",   "SLOW = 4x/SEC     ",  "MED  = 8x/SEC     ",  "FAST = 15x/SEC    "},
    {"FPS SETTING", {"OFF",         "ON", "ON-TURBO"},                  &showFps,               3,          "SHOW FPS ON MAIN  ",   "DISPLAY. OPTIONALY",  "RUN IN TURBO MODE ",  "FAST AS POSSIBLE  "},
    {"ARTIFACTING", {"OFF",         "1:BROWN/BLUE", "2:BLUE/BROWN", 
                                    "3:RED/GREEN","4:GREEN/RED"},       &global_artif_mode,     5,          "A FEW HIRES GAMES ",   "NEED ARTIFACING   ",  "TO LOOK RIGHT     ",  "OTHERWISE SET OFF "},
    {"BLENDING",    {"NORMAL",      "SHARP"},                           &jitter_type,           2,          "NORMAL WILL BLUR  ",   "THE SCREEN LIGHTLY",  "TO HELP SCALING   ",  "SHARP DOES NOT    "},
    {"DISK SPEEDUP",{"OFF",         "ON"},                              &ESC_enable_sio_patch,  2,          "NORMALLY ON IS    ",   "DESIRED TO SPEED  ",  "UP FLOPPY DISK    ",  "ACCESS. OFF=SLOW  "},
    {"KEY CLICK",   {"ON",          "OFF"},                             &key_click_disable,     2,          "NORMALLY ON       ",   "CAN BE USED TO    ",  "SILENCE KEY CLICKS",  "FOR KEYBOARD USE  "},
    {"EMULATOR TXT",{"OFF",         "ON"},                              &bShowEmuText,          2,          "NORMALLY ON       ",   "CAN BE USED TO    ",  "DISABLE FILENAME  ",  "INFO ON MAIN SCRN "},
    {"KEYBOARD",    {"NORMAL",      "SIMPLIFIED"},                      &keyboard_type,         2,          "NORMAL KEYBOARD   ",   "HAS MOST KEYS AND ",  "SIMPLIFIED IS MORE",  "STREAMLINED USE   "},
    
    {NULL,          {"",            ""},                                NULL,                   2,          "HELP1             ",   "HELP2             ",  "HELP3             ",  "HELP4             "},
};


void dsChooseOptions(int bOkayToChangePalette)
{
    static int last_pal=-1;
    static int last_art=-1;
    int optionHighlighted;
    int idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;
    char strBuf[64];

    // Show the background...
    decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    
    tv_type2 =  (tv_mode == TV_NTSC ? 0:1); 

    basic_opt = 0;
    if (bHaveBASIC) 
    {
        basic_opt = (basic_type == BASIC_ALTIRRA ? 1:2);
    }
    
    idx=0;
    while (true)
    {
        sprintf(strBuf, " %-12s  : %-12s ", Option_Table[idx].label, Option_Table[idx].option[*(Option_Table[idx].option_val)]);
        dsPrintValue(1,5+idx, (idx==0 ? 1:0), strBuf);
        idx++;
        if (Option_Table[idx].label == NULL) break;   
    }
    
    dsPrintValue(2,23, 0, "A=TOGGLE, B=EXIT, START=SAVE");
    optionHighlighted = 0;
    while (!bDone) 
    {
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                sprintf(strBuf, " %-12s  : %-12s ", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                sprintf(strBuf, " %-12s  : %-12s ", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,5+optionHighlighted,1, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                sprintf(strBuf, " %-12s  : %-12s ", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                sprintf(strBuf, " %-12s  : %-12s ", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,5+optionHighlighted,1, strBuf);
            }
            
            if (keysCurrent() & KEY_A)  // Toggle option
            {
                *(Option_Table[optionHighlighted].option_val) = (*(Option_Table[optionHighlighted].option_val) + 1) % Option_Table[optionHighlighted].option_max;
                sprintf(strBuf, " %-12s  : %-12s ", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,5+optionHighlighted,1, strBuf);
                if (strcmp(Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)], "ATARIXL.ROM")==0)
                {
                    if (!bAtariOS) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else if (strcmp(Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)], "ATARIOSB.ROM")==0)
                {
                    if (!bAtariOSB) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else if (strcmp(Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)], "ATARIBAS.ROM")==0)
                {
                    if (!bAtariBASIC) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else dsPrintValue(0,0,0,"           ");
            }
            if (keysCurrent() & KEY_START)  // Save Options
            {
                if (bOkayToChangePalette)   // This lets us know that a game is selected... 
                {
                    dsWriteFavs();
                }
            }
            if (keysCurrent() & KEY_B)  // Exit options
            {
                break;
            }
    
            // In case the Artifacting global changed....
            if (last_art != global_artif_mode)
            {
                last_art = global_artif_mode;
                ANTIC_UpdateArtifacting();                
            }
            // In case the Pallette global changed....
            if (last_pal != palett_type)
            {
                last_pal = palett_type;
                if (bOkayToChangePalette) dsSetAtariPalette();
            }

            dsPrintValue(14,0,0,Option_Table[optionHighlighted].help1);
            dsPrintValue(14,1,0,Option_Table[optionHighlighted].help2);
            dsPrintValue(14,2,0,Option_Table[optionHighlighted].help3);
            dsPrintValue(14,3,0,Option_Table[optionHighlighted].help4);
        }
        swiWaitForVBlank();
    }
  
    tv_mode = (tv_type2 == 0 ? TV_NTSC:TV_PAL);
    if (showFps == 2) full_speed=1; else full_speed=0;
    
    bHaveBASIC = (basic_opt ? 1:0);
    basic_type = (basic_opt == 2 ? BASIC_ATARIREVC:BASIC_ALTIRRA);
    
    if (ram_type == 0) ram_size = RAM_128K; 
    else if (ram_type == 1) ram_size = RAM_320_RAMBO;
    else ram_size = RAM_48K;

    // ---------------------------------------------------------------------------------------------
    // Sanity check... make sure if the user chose some odd combo of RAM and OS we fix it up...
    // ---------------------------------------------------------------------------------------------
    if (ram_size == RAM_48K) // If 48K... make sure we have OS set to one of the XLs
    {
        if ((os_type == OS_ALTIRRA_XL) || (os_type == OS_ATARI_XL))
        {
            os_type = OS_ALTIRRA_800;
        }
    }    
    else    // Must be 128K or 320K so make sure we aren't using older Atari 800 BIOS
    {
        if ((os_type == OS_ALTIRRA_800) || (os_type == OS_ATARI_OSB))
        {
            os_type = OS_ALTIRRA_XL;
        }
    }    
        
    install_os();    
    
    dsInstallSoundEmuFIFO();
    
    // Restore original bottom graphic
    decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
    dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
    
    dsShowRomInfo();
    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }
    
    return;    
}

char file_load_id[10];
bool bLoadReadOnly = true;
bool bLoadAndBoot = true;
void dsDisplayLoadOptions(void)
{
  char tmpBuf[32];
  
  dsPrintValue(0,0,0,file_load_id);
  sprintf(tmpBuf, "%-4s %s", (tv_mode == TV_NTSC ? "NTSC":"PAL"), (bHaveBASIC ? "W BASIC":"       "));
  dsPrintValue(19,0,0,tmpBuf);
  sprintf(tmpBuf, "[%c]  READ-ONLY", (bLoadReadOnly ? 'X':' '));
  dsPrintValue(14,1,0,tmpBuf);
  if (strcmp(file_load_id,"D2")!=0)      // For D2: we don't allow boot load
  {
      sprintf(tmpBuf, "[%c]  BOOT LOAD", (bLoadAndBoot ? 'Y':' '));
      dsPrintValue(14,2,0,tmpBuf);    
  }
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
  countfiles ? sprintf(szName,"%04d/%04d GAMES",(int)(1+ucSel+NoDebGame),countfiles) : sprintf(szName,"%04d/%04d FOLDERS",(int)(1+ucSel+NoDebGame),count8bit);
  dsPrintValue(14,3,0,szName);

  dsPrintValue(31,5,0,(char *) (NoDebGame>0 ? "<" : " "));
  dsPrintValue(31,22,0,(char *) (NoDebGame+14<count8bit ? ">" : " "));
  sprintf(szName,"%s","A=PICK B=BACK SEL=PAL STA=BASIC");
  dsPrintValue(0,23,0,szName);
  for (ucBcl=0;ucBcl<17; ucBcl++) 
  {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < count8bit) 
    {
      char szName2[300];
      maxLen=strlen(a8romlist[ucGame].filename);
      strcpy(szName,a8romlist[ucGame].filename);
      if (maxLen>29) szName[29]='\0';
      if (a8romlist[ucGame].directory) 
      {
        char szName3[36];
        sprintf(szName3,"[%s]",szName);
        sprintf(szName2,"%-29s",szName3);
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

char tmpBuf[80];
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
  
  nbRomPerPage = (count8bit>=17 ? 17 : count8bit);
  uNbRSPage = (count8bit>=5 ? 5 : count8bit);
  if (ucFicAct>count8bit)
  {
      firstRomDisplay=0;
      ucFicAct=0;
      romSelected=0;
  }
  if (ucFicAct>count8bit-nbRomPerPage) 
  {
    firstRomDisplay=count8bit-nbRomPerPage;
    romSelected=ucFicAct-count8bit+nbRomPerPage;
  }
  else 
  {
    firstRomDisplay=ucFicAct;
    romSelected=0;
  }

  dsDisplayFiles(firstRomDisplay,romSelected);
  while (!bDone) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        ucFicAct = (ucFicAct>0 ? ucFicAct-1 : count8bit-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=count8bit-nbRomPerPage;
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
      uLenFic=0; ucFlip=0;
    }
    else {
      ucHaut = 0;
    }  
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        ucFicAct = (ucFicAct< count8bit-1 ? ucFicAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<count8bit-nbRomPerPage) { firstRomDisplay += 1; }
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
      uLenFic=0; ucFlip=0;
    }
    else {
      ucBas = 0;
    }  
    if ((keysCurrent() & KEY_R) || (keysCurrent() & KEY_RIGHT)) {
      if (!ucSBas) {
        ucFicAct = (ucFicAct< count8bit-nbRomPerPage ? ucFicAct+nbRomPerPage : count8bit-nbRomPerPage);
        if (firstRomDisplay<count8bit-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = count8bit-nbRomPerPage; }
        if (ucFicAct == count8bit-nbRomPerPage) romSelected = 0;
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
        if (romSelected > ucFicAct) romSelected = ucFicAct;
          
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
    static int last_sel_key = 0;
    if (keysCurrent() & KEY_SELECT) 
    {
        if (last_sel_key != KEY_SELECT)
        {
            if (tv_mode == TV_NTSC) 
                tv_mode = TV_PAL;
            else
                tv_mode = TV_NTSC;
            dsDisplayLoadOptions();
            last_sel_key = KEY_SELECT;
        }
    } else last_sel_key=0;

    static int last_sta_key = 0;
    if (keysCurrent() & KEY_START) 
    {
        if (last_sta_key != KEY_START)
        {
            bHaveBASIC = 1-bHaveBASIC;
            dsDisplayLoadOptions();
            last_sta_key = KEY_START;
        }    
    } else last_sta_key=0;
      
    if (keysCurrent() & KEY_A) {
      if (!a8romlist[ucFicAct].directory) {
        bRet=true;
        bDone=true;
      }
      else {
        chdir(a8romlist[ucFicAct].filename);
        a8FindFiles();
        ucFicAct = 0;
        nbRomPerPage = (count8bit>=16 ? 16 : count8bit);
        uNbRSPage = (count8bit>=5 ? 5 : count8bit);
        if (ucFicAct>count8bit-nbRomPerPage) {
          firstRomDisplay=count8bit-nbRomPerPage;
          romSelected=ucFicAct-count8bit+nbRomPerPage;
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
        if (strcmp(file_load_id,"D2")!=0)      // For D2: we don't allow boot load
        {
            if (last_y_key != KEY_Y)
            {
                bLoadAndBoot = (bLoadAndBoot ? false:true);
                dsDisplayLoadOptions();
                last_y_key = KEY_Y;
            }
        }
    } else last_y_key = 0;      
      
    // Scroll la selection courante
    if (strlen(a8romlist[ucFicAct].filename) > 29) {
      ucFlip++;
      if (ucFlip >= 10) {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+29)>strlen(a8romlist[ucFicAct].filename)) {
          ucFlop++;
          if (ucFlop >= 10) {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,a8romlist[ucFicAct].filename+uLenFic,29);
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
    
  dsShowRomInfo();
  
  return bRet;
}

static u16 shift=0;
static u16 ctrl=0;
static u16 sav1 = 0;
static u16 sav2 = 0;
void dsShowKeyboard(void)
{
    if (keyboard_type == 0)
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
    else
    {
          decompress(bgKeyBriefTiles, bgGetGfxPtr(bg0b), LZ77Vram);
          decompress(bgKeyBriefMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
          dmaCopy((void *) bgKeyBriefPal,(u16*) BG_PALETTE_SUB,256*2);
          unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
          dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
          swiWaitForVBlank();
          sav1 = *(bgGetMapPtr(bg1b) + 680);
          sav2 = *(bgGetMapPtr(bg1b) + 683);
    }
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
  dsShowRomInfo();
}

unsigned int dsWaitOnMenu(unsigned int actState) 
{
  unsigned int uState=A8_PLAYINIT;
  unsigned int keys_pressed;
  bool bDone=false, romSel;
  bool bShowHelp=false;
  int iTx,iTy;
  
  while (!bDone) {
    // wait for stylus
    keys_pressed = keysCurrent();
    if (keys_pressed & KEY_TOUCH) 
    {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
        
      if (bShowHelp)
      {
        bShowHelp=false;
        dsRestoreBottomScreen();
      }
      else
      {
            if ((iTx>204) && (iTx<240) && (iTy>150) && (iTy<185))  // Gear Icon = Settings
            {     
                dsChooseOptions(FALSE);
            }
            else if ((iTx>230) && (iTx<256) && (iTy>8) && (iTy<30))  // POWER / QUIT
            { 
                soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                bDone=dsWaitOnQuit();
                if (bDone) uState=A8_QUITSTDS;
            }
            else if ((iTx>5) && (iTx<80) && (iTy>12) && (iTy<75)) // cartridge slot (wide range)
            {     
                bDone=true; 
                // Find files in current directory and show it 
                a8FindFiles();
                strcpy(file_load_id, "XEX/D1");
                romSel=dsWaitForRom();
                if (romSel) { uState=A8_PLAYINIT; 
                  dsLoadGame(a8romlist[ucFicAct].filename, DISK_1, bLoadAndBoot, bLoadReadOnly); }
                else { uState=actState; }
            }
            else if ((iTx>35) && (iTx<55) && (iTy>150) && (iTy<180))  // Help                
            { 
                dsShowHelp();
                bShowHelp = true;
                swiWaitForVBlank();swiWaitForVBlank();swiWaitForVBlank();swiWaitForVBlank();swiWaitForVBlank();                
            }
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

int dsHandleKeyBrief(int Tx, int Ty)
{
    int keyPress = AKEY_NONE;
    
    if (Ty > 0 && Ty < 35)
    {
        if (Tx <  37)       keyPress = AKEY_1;
        else if (Tx <  73)  keyPress = AKEY_2;
        else if (Tx <  110) keyPress = AKEY_3;
        else if (Tx <  147) keyPress = AKEY_4;
        else if (Tx <  184) keyPress = AKEY_5;
        else if (Tx <  220) keyPress = AKEY_0;
        else if (Tx <  256) keyPress = AKEY_BACKSPACE;
    }
    else if (Ty < 67)
    {
        if (Tx <  37)       keyPress = AKEY_a;
        else if (Tx <  73)  keyPress = AKEY_b;
        else if (Tx <  110) keyPress = AKEY_c;
        else if (Tx <  147) keyPress = AKEY_d;
        else if (Tx <  184) keyPress = AKEY_e;
        else if (Tx <  220) keyPress = AKEY_f;
        else if (Tx <  256) keyPress = AKEY_g;
    }
    else if (Ty < 98)
    {
        if (Tx <  37)       keyPress = AKEY_h;
        else if (Tx <  73)  keyPress = AKEY_i;
        else if (Tx <  110) keyPress = AKEY_j;
        else if (Tx <  147) keyPress = AKEY_k;
        else if (Tx <  184) keyPress = AKEY_l;
        else if (Tx <  220) keyPress = AKEY_m;
        else if (Tx <  256) keyPress = AKEY_n;
    }
    else if (Ty < 130)
    {
        if (Tx <  37)       keyPress = AKEY_o;
        else if (Tx <  73)  keyPress = AKEY_p;
        else if (Tx <  110) keyPress = AKEY_q;
        else if (Tx <  147) keyPress = AKEY_r;
        else if (Tx <  184) keyPress = AKEY_s;
        else if (Tx <  220) keyPress = AKEY_t;
        else if (Tx <  256) keyPress = AKEY_u;
    }
    else if (Ty < 162)
    {
        if (Tx <  37)       keyPress = AKEY_v;
        else if (Tx <  73)  keyPress = AKEY_w;
        else if (Tx <  110) keyPress = AKEY_x;
        else if (Tx <  147) keyPress = AKEY_y;
        else if (Tx <  184) keyPress = AKEY_z;
        else if (Tx <  220) keyPress = AKEY_FULLSTOP;
        else if (Tx <  256) keyPress = AKEY_COLON;
    }
    else
    {
        if (Tx <  52) keyPress = AKEY_EXIT;
        else if (Tx < 200) keyPress = AKEY_SPACE;
        else if (Tx < 256) keyPress = AKEY_RETURN;
    }
    
    return keyPress; 
}

int dsHandleKeyboard(int Tx, int Ty)
{
    int keyPress = AKEY_NONE;
    
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
        if (Tx <  30) keyPress = (shift ? AKEY_Q : AKEY_q);
        else if (Tx <  56) keyPress = (shift ? AKEY_W : AKEY_w);
        else if (Tx <  80) keyPress = (shift ? AKEY_E : AKEY_e);
        else if (Tx < 104) keyPress = (shift ? AKEY_R : AKEY_r);
        else if (Tx < 130) keyPress = (shift ? AKEY_T : AKEY_t);
        else if (Tx < 152) keyPress = (shift ? AKEY_Y : AKEY_y);
        else if (Tx < 179) keyPress = (shift ? AKEY_U : AKEY_u);
        else if (Tx < 204) keyPress = (shift ? AKEY_I : AKEY_i);
        else if (Tx < 229) keyPress = (shift ? AKEY_O : AKEY_o);
        else if (Tx < 255) keyPress = (shift ? AKEY_P : AKEY_p);
    }
    else if (Ty < 134)  // Home Row ASDF-JKL;
    {
        if (Tx <  30) keyPress = (shift ? AKEY_A : AKEY_a);
        else if (Tx <  56) keyPress = (shift ? AKEY_S : AKEY_s);
        else if (Tx <  80) keyPress = (shift ? AKEY_D : AKEY_d);
        else if (Tx < 104) keyPress = (shift ? AKEY_F : AKEY_f);
        else if (Tx < 130) keyPress = (shift ? AKEY_G : AKEY_g);
        else if (Tx < 152) keyPress = (shift ? AKEY_H : AKEY_h);
        else if (Tx < 179) keyPress = (shift ? AKEY_J : AKEY_j);
        else if (Tx < 204) keyPress = (shift ? AKEY_K : AKEY_k);
        else if (Tx < 229) keyPress = (shift ? AKEY_L : AKEY_l);
        else if (Tx < 255) keyPress = AKEY_SEMICOLON;
    }
    else if (Ty < 162)  // Bottom Row ZXCV...
    {
        if (Tx <  30) keyPress = (shift ? AKEY_Z : AKEY_z);
        else if (Tx <  56) keyPress = (shift ? AKEY_X : AKEY_x);
        else if (Tx <  80) keyPress = (shift ? AKEY_C : AKEY_c);
        else if (Tx < 104) keyPress = (shift ? AKEY_V : AKEY_v);
        else if (Tx < 130) keyPress = (shift ? AKEY_B : AKEY_b);
        else if (Tx < 152) keyPress = (shift ? AKEY_N : AKEY_n);
        else if (Tx < 179) keyPress = (shift ? AKEY_M : AKEY_m);
        else if (Tx < 204) keyPress = AKEY_COMMA;
        else if (Tx < 229) keyPress = AKEY_FULLSTOP;
        else if (Tx < 255) keyPress = AKEY_COLON;
    }
    else
    {
        if (Tx <  30) keyPress = AKEY_EXIT;
        else if (Tx <  56) keyPress = AKEY_NONE;
        else if (Tx <  80) keyPress = AKEY_SHFT;
        else if (Tx < 104) keyPress = AKEY_CTRL;
        else if (Tx < 130) keyPress = AKEY_BREAK;
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

ITCM_CODE void dsMainLoop(void) 
{
  static int last_key_code = -1;
  static bool bFirstLoad = true;
  char fpsbuf[32];
  unsigned int keys_pressed,keys_touch=0, romSel=0;
  int iTx,iTy;
  bool bShowHelp = false;
  bool bShowKeyboard = false;
  
  // Timers are fed with 33.513982 MHz clock.
  // With DIV_1024 the clock is 32,728.5 ticks per sec...
  TIMER0_DATA=0;
  TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;  
    
  myGame_offset_x = 32;
  myGame_offset_y = 24;
  myGame_scale_x = 256;
  myGame_scale_y = 256;
  
  while(etatEmu != A8_QUITSTDS) 
  {
    switch (etatEmu) 
    {
    
      case A8_MENUINIT:
        dsShowScreenMain();
        etatEmu = A8_MENUSHOW;
        break;
        
      case A8_MENUSHOW:
        etatEmu =  dsWaitOnMenu(A8_MENUSHOW);
        Atari800_Initialise();
        break;
        
      case A8_PLAYINIT:
        dsShowScreenEmu();
        irqEnable(IRQ_TIMER2);  
        etatEmu = A8_PLAYGAME;
        break;
        
      case A8_PLAYGAME:
        // 32,728.5 ticks = 1 second
        // 1 frame = 1/50 or 1/60 (0.02 or 0.016)
        // 655 -> 50 fps and 546 -> 60 fps
        if (!full_speed)
        {
            while(TIMER0_DATA < ((tv_mode == TV_NTSC ? 546:656)*atari_frames))
                ;
        }

        // Execute one frame
        Atari800_Frame();
            
        if (++atari_frames >= (tv_mode == TV_NTSC ? 60:50))
        {
            TIMER0_CR=0;
            TIMER0_DATA=0;
            TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024;
            atari_frames=0;
        }

        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= 32728)   // 1000MS (1 sec)
        {
            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
        
            if (gTotalAtariFrames == (tv_mode == TV_NTSC ? 61:51)) gTotalAtariFrames = (tv_mode == TV_NTSC ? 60:50);
            if (showFps) { siprintf(fpsbuf,"%03d",gTotalAtariFrames); dsPrintValue(0,0,0, fpsbuf); } // Show FPS
            if (full_speed) dsPrintValue(30,0,0,"FS");
            gTotalAtariFrames = 0;
            DumpDebugData();
            dsClearDiskActivity();
            if(bAtariCrash) dsPrintValue(1,23,0, "GAME CRASH - PICK ANOTHER GAME");
        }
        
        // Read keys
        keys_pressed=keysCurrent();
        key_consol = CONSOL_NONE;
            
        key_shift = 0;
        key_code = AKEY_NONE;
        if (bUseA_KeyAsUP && bUseB_KeyAsDN)
        {
            if (keys_pressed & KEY_A) keys_pressed |= KEY_UP;
            if (keys_pressed & KEY_B) keys_pressed |= KEY_DOWN;
        }
        else if (bUseA_KeyAsUP)
        {
            if (keys_pressed & KEY_A) keys_pressed |= KEY_UP;
            trig0 = (keys_pressed & KEY_B) ? 0 : 1;
        }
        else if (bUseB_KeyAsDN)
        {
            if (keys_pressed & KEY_B) keys_pressed |= KEY_DOWN;
            trig0 = (keys_pressed & KEY_A) ? 0 : 1;
        }
        else
        {
            trig0 = ((keys_pressed & KEY_A) || (keys_pressed & KEY_B)) ? 0 : 1;
        }
        stick0 = STICK_CENTRE;
        stick1 = STICK_CENTRE;
                  
        if (keys_pressed & KEY_Y) key_consol &= ~CONSOL_OPTION;
        if (keys_pressed & KEY_X) 
        {
            if (keys_pressed & KEY_L) key_code = AKEY_ESCAPE;
            else if (keys_pressed & KEY_R) key_code = AKEY_RETURN;
            else key_code = (bUseX_KeyAsCR ? AKEY_RETURN:AKEY_SPACE);
        }
            
        // if touch screen pressed
        if (keys_pressed & KEY_TOUCH) 
        {
            touchPosition touch;
            touchRead(&touch);
            iTx = touch.px;
            iTy = touch.py;
            debug[0]=iTx;
            debug[1]=iTy;
            
            // ---------------------------------------------------------------------------------------
            // START, SELECT and OPTION respond immediately - that is, we keep the buttons pressed 
            // as long as the user is continuing to hold down the button on the touch screen...
            // ---------------------------------------------------------------------------------------
            if (!(bShowHelp || bShowKeyboard))
            {
                if ((iTx>15) && (iTx<66) && (iTy>120) && (iTy<143))  // START
                { 
                    if (!keys_touch) 
                    {
                        soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    }
                    keys_touch=1;
                    key_consol &= ~CONSOL_START;
                }
                else if ((iTx>73) && (iTx<127) && (iTy>120) && (iTy<143))  // SELECT
                { 
                    if (!keys_touch)
                    {
                        soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    }
                    keys_touch=1;
                    key_consol &= ~CONSOL_SELECT;
                }
                else if ((iTx>133) && (iTx<186) && (iTy>120) && (iTy<143))  // OPTION
                { 
                    if (!keys_touch)
                    {
                        soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                    }
                    keys_touch=1;
                    key_consol &= ~CONSOL_OPTION;
                }
            }
            
            if (!keys_touch) 
            {
                if (bShowHelp || bShowKeyboard)
                {
                      if (bShowKeyboard)
                      {
                          if (keyboard_type == 0)
                            key_code = dsHandleKeyboard(iTx, iTy);
                          else
                            key_code = dsHandleKeyBrief(iTx, iTy);
                         if (key_code == AKEY_EXIT)
                         {
                              bShowKeyboard = false;
                              keys_touch = 1;
                              dsRestoreBottomScreen();
                              key_code = AKEY_NONE;
                         }
                         else if (key_code != AKEY_NONE)
                         {
                              if ((last_key_code == key_code) || (last_key_code == AKEY_NONE))
                              {
                                  if (last_key_code == AKEY_NONE)
                                  {
                                      if (!key_click_disable) soundPlaySample(keyclick_wav, SoundFormat_16Bit, keyclick_wav_size, 44100, 127, 64, false, 0);
                                      last_key_code = key_code;
                                  }                                  
                              }
                              else key_code = AKEY_NONE;
                         }
                         else // Must be AKEY_NONE
                         {
                             last_key_code = AKEY_NONE;
                         }
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
                    if ((iTx>=0) && (iTx<16) && (iTy>0) && (iTy<18))   // Show FPS
                    {
                       if (keys_touch == 0)
                       {
                           showFps = (showFps ? 0:1);
                           dsPrintValue(0,0,0, "   "); 
                           keys_touch = 1;
                       }
                    }
                    else if ((iTx>192) && (iTx<250) && (iTy>120) && (iTy<143))  // RESET (just reloads the game... not sure what else to do here)
                    { 
                        if (!keys_touch)
                        {
                            soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                        }
                        keys_touch = 1;
                        dsLoadGame(last_filename, DISK_1, true, bLoadReadOnly);   // Force Restart 
                        irqEnable(IRQ_TIMER2); 
                        fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
                    }
                    else if ((iTx>35) && (iTx<55) && (iTy>150) && (iTy<180))  // Help
                    { 
                        if (!keys_touch) 
                        {
                            soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                        }
                        dsShowHelp();
                        bShowHelp = true;
                        keys_touch = 1;
                    }
                    else if ((iTx>88) && (iTx<175) && (iTy>150) && (iTy<180))  // Keyboard
                    { 
                        if (!keys_touch) 
                        {
                            soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                        }
                        dsShowKeyboard();
                        bShowKeyboard = true;
                        keys_touch = 1;
                    }
                    else if ((iTx>230) && (iTx<256) && (iTy>8) && (iTy<30))  // POWER / QUIT
                    { 
                      irqDisable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (0) | SOUND_SET_VOLUME);
                      soundPlaySample(clickNoQuit_wav, SoundFormat_16Bit, clickNoQuit_wav_size, 22050, 127, 64, false, 0);
                      if (dsWaitOnQuit()) etatEmu=A8_QUITSTDS;
                      else { irqEnable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME); }
                    }
                    else if ((iTx>204) && (iTx<235) && (iTy>150) && (iTy<180)) {     // Gear Icon = Settings
                      irqDisable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (0) | SOUND_SET_VOLUME);
                      keys_touch=1;
                      dsChooseOptions(TRUE);
                      irqEnable(IRQ_TIMER2);
                      fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
                    }
                    else if ((iTx>5) && (iTx<80) && (iTy>12) && (iTy<75)) {     // XEX and D1 Disk Drive
                      irqDisable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (0) | SOUND_SET_VOLUME);
                      // Find files in current directory and show it 
                      keys_touch=1;
                      strcpy(file_load_id, "XEX/D1");
                      a8FindFiles();
                      romSel=dsWaitForRom();
                      if (romSel) { etatEmu=A8_PLAYINIT; dsLoadGame(a8romlist[ucFicAct].filename, DISK_1, bLoadAndBoot, bLoadReadOnly); }
                      else { irqEnable(IRQ_TIMER2); }
                      fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
                    }
                    else if ((iTx>5) && (iTx<80) && (iTy>77) && (iTy<114)) {     // D2 Disk Drive
                      irqDisable(IRQ_TIMER2); fifoSendValue32(FIFO_USER_01,(1<<16) | (0) | SOUND_SET_VOLUME);
                      // Find files in current directory and show it 
                      keys_touch=1;
                      strcpy(file_load_id, "D2");
                      a8FindFiles();
                      romSel=dsWaitForRom();
                      if (romSel) { etatEmu=A8_PLAYINIT; dsLoadGame(a8romlist[ucFicAct].filename, DISK_2, false, bLoadReadOnly); }
                      else { irqEnable(IRQ_TIMER2); }
                      fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
                    }
                 }
            }
        }
        else 
        {
            last_key_code = -1;
            keys_touch=0;
        }
      
        // Only handle UP/DOWN/LEFT/RIGHT if shoulder buttons are not pressed (those are handled below)
        if ((keys_pressed & (KEY_R|KEY_L)) == 0)
        {
            if (keys_pressed & KEY_UP) stick0 = STICK_FORWARD;
            if (keys_pressed & KEY_LEFT) stick0 = STICK_LEFT;
            if (keys_pressed & KEY_RIGHT) stick0 = STICK_RIGHT;
            if (keys_pressed & KEY_DOWN) stick0 = STICK_BACK;
            if ((keys_pressed & KEY_UP) && (keys_pressed & KEY_LEFT)) stick0 = STICK_UL; 
            if ((keys_pressed & KEY_UP) && (keys_pressed & KEY_RIGHT)) stick0 = STICK_UR;
            if ((keys_pressed & KEY_DOWN) && (keys_pressed & KEY_LEFT)) stick0 = STICK_LL;
            if ((keys_pressed & KEY_DOWN) && (keys_pressed & KEY_RIGHT)) stick0 = STICK_LR;
        }

        if (keys_pressed & KEY_START) key_consol &= ~CONSOL_START;
        if (keys_pressed & KEY_SELECT) key_consol &= ~CONSOL_SELECT;
        if (gTotalAtariFrames & 1)  // Every other frame...
        {
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_L))
            {
                dsWriteFavs();
            }
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_UP))   myGame_offset_y++;
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_DOWN)) myGame_offset_y--;
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_LEFT))  myGame_offset_x++;
            if ((keys_pressed & KEY_R) && (keys_pressed & KEY_RIGHT)) myGame_offset_x--;

            if ((keys_pressed & KEY_L) && (keys_pressed & KEY_UP))   if (myGame_scale_y <= 256) myGame_scale_y++;
            if ((keys_pressed & KEY_L) && (keys_pressed & KEY_DOWN)) if (myGame_scale_y >= 192) myGame_scale_y--;
            if ((keys_pressed & KEY_L) && (keys_pressed & KEY_RIGHT))  if (myGame_scale_x < 320) myGame_scale_x++;
            if ((keys_pressed & KEY_L) && (keys_pressed & KEY_LEFT)) if (myGame_scale_x >= 192) myGame_scale_x--;
        }            
           
        // A bit of a hack... the first load requires a cold restart after the OS is running....
        if (bFirstLoad)
        {
            bFirstLoad = false;
            dsLoadGame(last_filename, DISK_1, true, bLoadReadOnly);   // Force Restart 
            irqEnable(IRQ_TIMER2); 
            fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
        }

        break;
    }
  }
}

//----------------------------------------------------------------------------------
// Find files game available. We sort them directories first and then alphabetical.
//----------------------------------------------------------------------------------
int a8Filescmp (const void *c1, const void *c2) {
  FICA_A8 *p1 = (FICA_A8 *) c1;
  FICA_A8 *p2 = (FICA_A8 *) c2;
  if (p1->filename[0] == '.' && p2->filename[0] != '.')
      return -1;
  if (p2->filename[0] == '.' && p1->filename[0] != '.')
      return 1;
  if (p1->directory && !(p2->directory))
      return -1;
  if (p2->directory && !(p1->directory))
      return 1;
  return strcasecmp (p1->filename, p2->filename);
}

void a8FindFiles(void) 
{
  static bool bFirstTime = true;
  DIR *pdir;
  struct dirent *pent;
  char filenametmp[300];
  
  count8bit = countfiles= 0;
  
  // First time load... get into the root directory for easy navigation...
  if (bFirstTime)
  {
    bFirstTime = false;
    //TODO: Not sure on this yet...    chdir("/");
  }
  pdir = opendir(".");

  if (pdir) {

    while (((pent=readdir(pdir))!=NULL)) 
    {
      if (count8bit > 1023) break;
      strcpy(filenametmp,pent->d_name);
      if (pent->d_type == DT_DIR)
      {
        if (!( (filenametmp[0] == '.') && (strlen(filenametmp) == 1))) {
          a8romlist[count8bit].directory = true;
          strcpy(a8romlist[count8bit].filename,filenametmp);
          count8bit++;
        }
      }
      else 
      {
          if (strcmp(file_load_id,"D2")!=0)      // For D2: we don't load .xex
          {
              if ( (strcasecmp(strrchr(filenametmp, '.'), ".xex") == 0) )  {
                a8romlist[count8bit].directory = false;
                strcpy(a8romlist[count8bit].filename,filenametmp);
                count8bit++;countfiles++;
              }
          }
          if ( (strcasecmp(strrchr(filenametmp, '.'), ".atr") == 0) )  {
            a8romlist[count8bit].directory = false;
            strcpy(a8romlist[count8bit].filename,filenametmp);
            count8bit++;countfiles++;
          }
      }
    }
    closedir(pdir);
  }
  if (count8bit)
  {
    qsort (a8romlist, count8bit, sizeof (FICA_A8), a8Filescmp);
  }
  else  // Failsafe... always provide a back directory...
  {
    a8romlist[count8bit].directory = true;
    strcpy(a8romlist[count8bit].filename,"..");
    count8bit = 1;
  }
}

#define MAX_GAME_SETTINGS       1870
#define GAME_DATABASE_VERSION   0x05
struct GameSettings_t
{
    char game_hash[32];
    UBYTE slot_used;
    UBYTE tv_type;
    UBYTE pallete_type;
    UBYTE ram_type;
    UBYTE os_type;
    UBYTE basic_opt;
    UBYTE skip_frames;
    UBYTE aButtonMap;
    UBYTE xButtonMap;
    UBYTE auto_fire;
    UBYTE artifacting;
    UBYTE blending;
    short int xOffset;
    short int yOffset;
    short int xScale;
    short int yScale;
    UBYTE bButtonMap;
    UBYTE key_click_disable;
    short int keyboard_type;
    short int spare3;
    short int spare4;
    short int spare5;
    short int spare6;
    short int spare7;
    short int spare8;
    short int spare9;
};

struct GameDatabase_t
{
    UBYTE                       db_version;
    struct GameSettings_t       GameSettings[MAX_GAME_SETTINGS];
    unsigned int                checksum;
};

struct GameDatabase_t GameDB;

void InitGameSettings(void)
{
    // --------------------------------------------------
    // This will set every byte to 0x00 but then we
    // map a few specific things below...
    // --------------------------------------------------
    memset(&GameDB, 0x00, sizeof(GameDB));
    for (int i=0; i<MAX_GAME_SETTINGS; i++)
    {
        GameDB.GameSettings[i].slot_used = 0;
        GameDB.GameSettings[i].spare3 = 0;
        GameDB.GameSettings[i].spare4 = 0;
        GameDB.GameSettings[i].spare5 = 0;
        GameDB.GameSettings[i].spare6 = 0;
        GameDB.GameSettings[i].spare7 = 1;     // Map a few spares with a default of '1' which may come in handy in the future....
        GameDB.GameSettings[i].spare8 = 1;
        GameDB.GameSettings[i].spare9 = 1;
    }
}

void WriteGameSettings(void)
{
    FILE *fp;
    int idx = 0;
    
    GameDB.db_version = GAME_DATABASE_VERSION;
    // Search through the Game Database to see if we have a match to our game filename....
    for (idx=0; idx < MAX_GAME_SETTINGS; idx++)
    {
        if (GameDB.GameSettings[idx].slot_used == 0) break;
        if (memcmp (GameDB.GameSettings[idx].game_hash, last_hash, 32) == 0) break;
    }
     
    if (idx < MAX_GAME_SETTINGS)
    {
        GameDB.GameSettings[idx].slot_used = 1;
        memcpy(GameDB.GameSettings[idx].game_hash, last_hash, 32);
        GameDB.GameSettings[idx].tv_type = tv_type2;
        GameDB.GameSettings[idx].pallete_type = palett_type;
        GameDB.GameSettings[idx].os_type = os_type;
        GameDB.GameSettings[idx].basic_opt = basic_opt;
        GameDB.GameSettings[idx].auto_fire = auto_fire;
        GameDB.GameSettings[idx].skip_frames = skip_frames;
        GameDB.GameSettings[idx].ram_type = ram_type;
        GameDB.GameSettings[idx].artifacting = global_artif_mode;
        GameDB.GameSettings[idx].xOffset = myGame_offset_x;
        GameDB.GameSettings[idx].yOffset = myGame_offset_y;
        GameDB.GameSettings[idx].xScale = myGame_scale_x;
        GameDB.GameSettings[idx].yScale = myGame_scale_y;
        GameDB.GameSettings[idx].aButtonMap = bUseA_KeyAsUP;
        GameDB.GameSettings[idx].bButtonMap = bUseB_KeyAsDN;
        GameDB.GameSettings[idx].xButtonMap = bUseX_KeyAsCR;
        GameDB.GameSettings[idx].blending   = jitter_type;
        GameDB.GameSettings[idx].key_click_disable = key_click_disable;
        GameDB.GameSettings[idx].keyboard_type = keyboard_type;  
        
        GameDB.checksum = 0;
        char *ptr = (char *)GameDB.GameSettings;
        for (int i=0; i<sizeof(GameDB.GameSettings); i++)
        {
               GameDB.checksum += *ptr;
        }
           
        DIR* dir = opendir("/data");
        if (dir)
        {
            /* Directory exists. */
            closedir(dir);
        }
        else
        {
            mkdir("/data", 0777);
        }
        fp = fopen("/data/XEGS-DS.DAT", "wb+");
        if (fp != NULL)
        {
            fwrite(&GameDB, sizeof(GameDB), 1, fp);
            fclose(fp);
        }
    }
}

void ReadGameSettings(void)
{
    FILE *fp;
        
    fp = fopen("/data/XEGS-DS.DAT", "rb");
    if (fp != NULL)
    {
        fread(&GameDB, sizeof(GameDB), 1, fp);
        fclose(fp);
        
        unsigned int checksum = 0;
        char *ptr = (char *)GameDB.GameSettings;
        for (int i=0; i<sizeof(GameDB.GameSettings); i++)
        {
               checksum += *ptr;
        }        
        if ((GameDB.db_version != GAME_DATABASE_VERSION) || (GameDB.checksum != checksum))
        {
            InitGameSettings();       
        }
    }
    else
    {
        InitGameSettings();   
    }
}

void ApplyGameSpecificSettings(void)
{
    int idx = 0;
    
    // Search through the Game Database to see if we have a match to our game filename....
    for (idx=0; idx < MAX_GAME_SETTINGS; idx++)
    {
        if (memcmp(GameDB.GameSettings[idx].game_hash, last_hash, 32) == 0) break;
    }

    full_speed = 0;  
    if (idx < MAX_GAME_SETTINGS)
    {
      myGame_offset_x   = GameDB.GameSettings[idx].xOffset;
      myGame_offset_y   = GameDB.GameSettings[idx].yOffset;
      myGame_scale_x    = GameDB.GameSettings[idx].xScale;
      myGame_scale_y    = GameDB.GameSettings[idx].yScale;
      tv_mode           = (GameDB.GameSettings[idx].tv_type == 0 ? TV_NTSC:TV_PAL);
      global_artif_mode = GameDB.GameSettings[idx].artifacting;
      palett_type       = GameDB.GameSettings[idx].pallete_type;
      os_type           = GameDB.GameSettings[idx].os_type;
      basic_opt         = GameDB.GameSettings[idx].basic_opt;
      bHaveBASIC        = (basic_opt ? 1:0);
      basic_type        = (basic_opt == 2 ? BASIC_ATARIREVC:BASIC_ALTIRRA);        
      auto_fire         = GameDB.GameSettings[idx].auto_fire;
      skip_frames       = GameDB.GameSettings[idx].skip_frames;
      ram_type          = GameDB.GameSettings[idx].ram_type;
      bUseA_KeyAsUP     = GameDB.GameSettings[idx].aButtonMap;
      bUseB_KeyAsDN     = GameDB.GameSettings[idx].bButtonMap;
      bUseX_KeyAsCR     = GameDB.GameSettings[idx].xButtonMap;
      jitter_type       = GameDB.GameSettings[idx].blending;
      key_click_disable = GameDB.GameSettings[idx].key_click_disable;  
      keyboard_type     = GameDB.GameSettings[idx].keyboard_type;  
      install_os();        
    }
    else
    {
      myGame_offset_x = 32;
      myGame_offset_y = 24;
      myGame_scale_x = 256;
      myGame_scale_y = 256;
      global_artif_mode=0;
      skip_frames = (isDSiMode() ? 0:1);   // For older DS models, we skip frames to get full speed...        
      auto_fire=0;
      bUseA_KeyAsUP=0;
      bUseB_KeyAsDN=0;
      bUseX_KeyAsCR=0;
      key_click_disable = 0;
      // Never default BASIC, OS, TV-TYPE or MEMORY!
    }
}
/*
 * INTRO.C contains the splash screen which shows briefly as the emulator loads.
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
#include <nds.h>
#include <stdio.h>

#include "a8ds.h"

#include "mus_intro_wav.h"
#include "pdev_tbg0.h"
#include "pdev_bg0.h"

volatile unsigned int vusCptVBL;

void vblankIntro() 
{
    vusCptVBL++;
}

// --------------------------------------------------------------------------
// Intro with portabledev logo... Alekmaul did the initial translation 
// of the Atari 800 emulator to the Nintendo DS and without that effort,
// this new A8DS version would not exist... so credit where credit due!
// --------------------------------------------------------------------------
void intro_logo(void) 
{
    int soundId=-1; 
    bool bOK;

    // Init graphics
    videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE );
    videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE );
    vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
    irqSet(IRQ_VBLANK, vblankIntro);
    irqEnable(IRQ_VBLANK);

    // Init BG
    int bg1 = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

    // Init sub BG
    int bg1s = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);

    REG_BLDCNT = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY = 16;
    REG_BLDCNT_SUB = BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0; REG_BLDY_SUB = 16;

    // Show portabledev
    decompress(pdev_tbg0Tiles, bgGetGfxPtr(bg1), LZ77Vram);
    decompress(pdev_tbg0Map, (void*) bgGetMapPtr(bg1), LZ77Vram);
    dmaCopy((void *) pdev_tbg0Pal,(u16*) BG_PALETTE,256*2);

    decompress(pdev_bg0Tiles, bgGetGfxPtr(bg1s), LZ77Vram);
    decompress(pdev_bg0Map, (void*) bgGetMapPtr(bg1s), LZ77Vram);
    dmaCopy((void *) pdev_bg0Pal,(u16*) BG_PALETTE_SUB,256*2);

    FadeToColor(0,BLEND_FADE_BLACK | BLEND_SRC_BG0 | BLEND_DST_BG0,3,0,3);

    soundId = soundPlaySample((const void *) mus_intro_wav, SoundFormat_16Bit, mus_intro_wav_size, 11025, 127, 64, false, 0);

    bOK=false;
    while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; } // 0x1FFF = key or pen
    vusCptVBL=0;bOK=false;
    while (!bOK && (vusCptVBL<3*60)) { if (keysCurrent() & 0x1FFF ) bOK=true; }
    bOK=false;
    while (!bOK) { if ( !(keysCurrent() & 0x1FFF) ) bOK=true; }

    FadeToColor(1,BLEND_FADE_WHITE | BLEND_SRC_BG0 | BLEND_DST_BG0,3,16,3);
    if (soundId!=-1) soundKill(soundId);
}

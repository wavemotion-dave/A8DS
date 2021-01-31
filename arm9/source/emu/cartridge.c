/*
 * cartridge.c - cartridge emulation
 *
 * Copyright (C) 2001-2003 Piotr Fusik
 * Copyright (C) 2001-2005 Atari800 development team (see DOC/CREDITS)
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "a5200utils.h"
#include "hash.h"

#include "atari.h"
#include "binload.h" /* loading_basic */
#include "cartridge.h"
#include "memory.h"
#include "pia.h"
#include "rtime.h"
#include "util.h"
#ifndef BASIC
#include "statesav.h"
#endif

struct cart_t cart_table[] = 
{
    {"45f8841269313736489180c8ec3e9588",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Activision Decathlon, The (USA).a52
    {"4b1aecab0e2f9c90e514cb0a506e3a5f",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Adventure II-a.a52
    {"e2f6085028eb8cf24ad7b50ca4ef640f",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Adventure II-b.a52
    {"bae7c1e5eb04e19ef8d0d0b5ce134332",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Astro Chase (USA).a52
    {"f5cd178cbea0ae7d8cf65b30cfd04225",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Ballblazer (USA).a52
    {"1913310b1e44ad7f3b90aeb16790a850",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Beamrider (USA).a52
    {"f8973db8dc272c2e5eb7b8dbb5c0cc3b",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,   6, 220,    256,    240,    32,20},  // BerZerk (USA).a52
    {"5d5a75ac53230915701bc918249f1055",    CART_5200_40,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Bounty Bob Strikes Back! (USA).a52
    {"a074a1ff0a16d1e034ee314b85fa41e9",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Buck Rogers - Planet of Zoom (USA).a52
    {"01b978c3faf5d516f300f98c00377532",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Carol Shaw's River Raid (USA).a52
    {"4965b4c8acca64c4fac39a7c0763f611",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Castle Blast (USA) (Unl).a52
    {"8f4c07a9e0ef2ded720b403810220aaf",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Castle Crisis (USA) (Unl).a52
    {"261702e8d9acbf45d44bb61fd8fa3e17",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    240,    32,14},  // Centipede (USA).a52
    {"3ff7707e25359c9bcb2326a5d8539852",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Choplifter! (USA).a52
    {"5720423ebd7575941a1586466ba9beaf",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Congo Bongo (USA).a52
    {"1a64edff521608f9f4fa9d7bdb355087",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Countermeasure (USA).a52
    {"7c27d225a13e178610babf331a0759c0",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // David Crane's Pitfall II - Lost Caverns (USA).a52
    {"27d5f32b0d46d3d80773a2b505f95046",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Defender (1982) (Atari).a52
    {"3abd0c057474bad46e45f3d4e96eecee",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  6,  220,    256,    192,    32, 0},  // Dig Dug (1983) (Atari).a52
    {"159ccaa564fc2472afd1f06665ec6d19",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Dreadnaught Factor, The (USA).a52
    {"14bd9a0423eafc3090333af916cfbce6",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Frisky Tom (USA) (Proto).a52
    {"2c89c9444f99fd7ac83f88278e6772c6",    CART_5200_8,        CTRL_FROG,  DIGITAL,    2,  6,  220,    256,    230,    32,14},  // Frogger (1983) (Parker Bros).a52
    {"d8636222c993ca71ca0904c8d89c4411",    CART_5200_EE_16,    CTRL_FROG,  DIGITAL,    2,  6,  220,    256,    192,    32, 0},  // Frogger II - Threeedeep! (USA).a52
    {"3ace7c591a88af22bac0c559bbb08f03",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Galaxian (1982) (Atari).a52
    {"85fe2492e2945015000272a9fefc06e3",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Gorf (1982) (CBS).a52
    {"dc271e475b4766e80151f1da5b764e52",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Gremlins (USA).a52
    {"dacc0a82e8ee0c086971f9d9bac14127",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Gyruss (USA).a52
    {"f8f0e0a6dc2ffee41b2a2dd736cba4cd",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // H.E.R.O. (USA).a52
    {"936db7c08e6b4b902c585a529cb15fc5",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // James Bond 007 (USA).a52
    {"25cfdef5bf9b126166d5394ae74a32e7",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Joust (USA).a52
    {"bc748804f35728e98847da6cdaf241a7",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Jr. Pac-Man (USA) (Proto).a52
    {"834067fdce5d09b86741e41e7e491d6c",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Jungle Hunt (USA).a52
    {"92fd2f43bc0adf2f704666b5244fadf1",    CART_5200_4,        CTRL_JOY,   ANALOG,     2,  30, 185,    256,    192,    32, 0},  // Kaboom! (USA).a52
    {"796d2c22f8205fb0ce8f1ee67c8eb2ca",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Kangaroo (USA).a52
    {"f25a084754ea4d37c2fb1dc8ca6dc51b",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Keystone Kapers (USA).a52
    {"3b03e3cda8e8aa3beed4c9617010b010",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Koffi - Yellow Kopter (USA) (Unl).a52
    {"b99f405de8e7700619bcd18524ba0e0e",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // K-Razy Shoot-Out (USA).a52
    {"46264c86edf30666e28553bd08369b83",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Last Starfighter, The (USA) (Proto).a52
    {"ff785ce12ad6f4ca67f662598025c367",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Megamania (1983) (Activision).a52
    {"1cd67468d123219201702eadaffd0275",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Meteorites (USA).a52
    {"84d88bcdeffee1ab880a5575c6aca45e",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Millipede (USA) (Proto).a52
    {"d859bff796625e980db1840f15dec4b5",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Miner 2049er Starring Bounty Bob (USA).a52
    {"972b6c0dbf5501cacfdc6665e86a796c",    CART_5200_8,        CTRL_JOY,   ANALOG,     2,  30, 185,    256,    192,    32, 0},  // Missile Command (USA).a52
    {"694897cc0d98fcf2f59eef788881f67d",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Montezuma's Revenge featuring Panama Joe (USA).a52
    {"296e5a3a9efd4f89531e9cf0259c903d",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Moon Patrol (USA).a52
    {"618e3eb7ae2810768e1aefed1bfdcec4",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Mountain King (USA).a52
    {"d1873645fee21e84b25dc5e939d93e9b",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Mr. Do!'s Castle (USA).a52
    {"ef9a920ffdf592546499738ee911fc1e",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  6,  220,    256,    192,    32, 0},  // Ms. Pac-Man (USA).a52
    {"f1a4d62d9ba965335fa13354a6264623",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  6,  220,    256,    192,    32, 0},  // Pac-Man (USA).a52
    {"a301a449fc20ad345b04932d3ca3ef54",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Pengo (USA).a52
    {"ecbd6dd2ab105dd43f98476966bbf26c",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Pitfall! (USA).a52
    {"fd0cbea6ad18194be0538844e3d7fdc9",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Pole Position (USA).a52
    {"dd4ae6add63452aafe7d4fa752cd78ca",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Popeye (USA).a52
    {"ce44d14341fcc5e7e4fb7a04f77ffec9",    CART_5200_8,        CTRL_QBERT, DIGITAL,    2,  6,  220,    300,    240,    48,20},  // Q-bert (USA).a52
    {"9b7d9d874a93332582f34d1420e0f574",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // QIX (USA).a52
    {"099706cedd068aced7313ffa371d7ec3",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Quest for Quintana Roo (USA).a52
    {"2bb928d7516e451c6b0159ac413407de",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // RealSports Baseball (USA).a52
    {"e056001d304db597bdd21b2968fcc3e6",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // RealSports Basketball (USA).a52
    {"022c47b525b058796841134bb5c75a18",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // RealSports Football (USA).a52
    {"3074fad290298d56c67f82e8588c5a8b",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // RealSports Soccer (USA).a52
    {"7e683e571cbe7c77f76a1648f906b932",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // RealSports Tennis (USA).a52
    {"0dc44c5bf0829649b7fec37cb0a8186b",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Rescue on Fractalus! (USA).a52
    {"ddf7834a420f1eaae20a7a6255f80a99",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Road Runner (USA) (Proto).a52
    {"5dba5b478b7da9fd2c617e41fb5ccd31",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Robotron 2084 (USA).a52
    {"be75afc33f5da12974900317d824f9b9",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Sinistar.a52
    {"6e24e3519458c5cb95a7fd7711131f8d",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Space Dungeon (USA).a52
    {"58430368d2c9190083f95ce923f4c996",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Space Invaders (USA).a52
    {"802a11dfcba6229cc2f93f0f3aaeb3aa",    CART_5200_NS_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Space Shuttle - A Journey Into Space (USA).a52
    {"e2d3a3e52bb4e3f7e489acd9974d68e2",    CART_5200_EE_16,    CTRL_JOY,   ANALOG,     2,  30, 185,    256,    192,    32, 0},  // Star Raiders (USA).a52
    {"c959b65be720a03b5479650a3af5a511",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Star Trek - Strategic Operations Simulator (USA).a52
    {"00beaa8405c7fb90d86be5bb1b01ea66",    CART_5200_EE_16,    CTRL_JOY,   ANALOG,     2,  30, 185,    256,    192,    32, 0},  // Star Wars - The Arcade Game (USA).a52
    {"865570ff9052c1704f673e6222192336",    CART_5200_4,        CTRL_JOY,   ANALOG,     3,  30, 185,    256,    192,    32, 0},  // Super Breakout (USA).a52
    {"dfcd77aec94b532728c3d1fef1da9d85",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Super Cobra (USA).a52
    {"556a66d6737f0f793821e702547bc051",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Vanguard (USA).a52
    {"560b68b7f83077444a57ebe9f932905a",    CART_5200_NS_16,    CTRL_SWAP,  DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Wizard of Wor (USA).a52
    {"9fee054e7d4ba2392f4ba0cb73fc99a5",    CART_5200_32,       CTRL_JOY,   DIGITAL,    2,  6,  220,    256,    220,    32,10},  // Zaxxon (USA).a52
    {"77beee345b4647563e20fd896231bd47",    CART_5200_8,        CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Zenji (USA).a52
    {"dc45af8b0996cb6a94188b0be3be2e17",    CART_5200_EE_16,    CTRL_JOY,   DIGITAL,    2,  30, 185,    256,    192,    32, 0},  // Zone Ranger (USA).a52
    {"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",    CART_NONE,          CTRL_JOY,   256,    192,    32, 0},  // End of list
};


int cart_kb[CART_LAST_SUPPORTED + 1] = {
        0,
        0,//8,  /* CART_STD_8 */
        0,//16, /* CART_STD_16 */
        0,//16, /* CART_OSS_16 */
    32, /* CART_5200_32 */
        0,//32, /* CART_DB_32 */
    16, /* CART_5200_EE_16 */
    40, /* CART_5200_40 */
        0,//64, /* CART_WILL_64 */
        0,//64, /* CART_EXP_64 */
        0,//64, /* CART_DIAMOND_64 */
        0,//64, /* CART_SDX */
        0,//32, /* CART_XEGS_32 */
        0,//64, /* CART_XEGS_64 */
        0,//128,/* CART_XEGS_128 */
        0,//16, /* CART_OSS2_16 */
    16, /* CART_5200_NS_16 */
        0,//128,/* CART_ATRAX_128 */
        0,//40, /* CART_BBSB_40 */
    8,  /* CART_5200_8 */
    4,  /* CART_5200_4 */
        0,//8,  /* CART_RIGHT_8 */
        0,//32, /* CART_WILL_32 */
        0,//256,/* CART_XEGS_256 */
        0,//512,/* CART_XEGS_512 */
    0,//1024,/* CART_XEGS_1024 */
    0,//16, /* CART_MEGA_16 */
    0,//32, /* CART_MEGA_32 */
    0,//64, /* CART_MEGA_64 */
    0,//128,/* CART_MEGA_128 */
    0,//256,/* CART_MEGA_256 */
    0,//512,/* CART_MEGA_512 */
    0,//1024,/* CART_MEGA_1024 */
    0,//32, /* CART_SWXEGS_32 */
    0,//64, /* CART_SWXEGS_64 */
    0,//128,/* CART_SWXEGS_128 */
    0,//256,/* CART_SWXEGS_256 */
    0,//512,/* CART_SWXEGS_512 */
    0,//1024,/* CART_SWXEGS_1024 */
    0,//8,  /* CART_PHOENIX_8 */
    0,//16, /* CART_BLIZZARD_16 */
    0,//128,/* CART_ATMAX_128 */
    0,//1024 /* CART_ATMAX_1024 */
};

int CART_IsFor5200(int type)
{
    switch (type) {
    case CART_5200_32:
    case CART_5200_EE_16:
    case CART_5200_40:
    case CART_5200_NS_16:
    case CART_5200_8:
    case CART_5200_4:
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

UBYTE *cart_image = NULL;       /* For cartridge memory */
char cart_filename[FILENAME_MAX];

struct cart_t myCart = {"", CART_5200_32, CTRL_JOY, 0,0,0,0};

static int bank;

/* DB_32, XEGS_32, XEGS_64, XEGS_128, XEGS_256, XEGS_512, XEGS_1024 */
/* SWXEGS_32, SWXEGS_64, SWXEGS_128, SWXEGS_256, SWXEGS_512, SWXEGS_1024 */
static void set_bank_809F(int b, int main)
{
    if (b != bank) {
        if (b & 0x80) {
            Cart809F_Disable();
            CartA0BF_Disable();
        }
        else {
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image + b * 0x2000);
            if (bank & 0x80)
                CopyROM(0xa000, 0xbfff, cart_image + main);
        }
        bank = b;
    }
}

/* OSS_16, OSS2_16 */
static void set_bank_A0AF(int b, int main)
{
    if (b != bank) {
        if (b < 0)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xafff, cart_image + b * 0x1000);
            if (bank < 0)
                CopyROM(0xb000, 0xbfff, cart_image + main);
        }
        bank = b;
    }
}

/* EXP_64, DIAMOND_64, SDX_64 */
static void set_bank_A0BF(int b)
{
    if (b != bank) {
        if (b & 0x0008)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image + (~b & 7) * 0x2000);
        }
        bank = b;
    }
}

static void set_bank_A0BF_WILL64(int b)
{
    if (b != bank) {
        if (b & 0x0008)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image + (b & 7) * 0x2000);
        }
        bank = b;
    }
}

static void set_bank_A0BF_WILL32(int b)
{
    if (b != bank) {
        if (b & 0x0008)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image + (b & 3) * 0x2000);
        }
        bank = b;
    }
}

static void set_bank_A0BF_ATMAX128(int b)
{
    if (b != bank) {
        if (b >= 0x20)
            return;
        else if (b >= 0x10)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image + b * 0x2000);
        }
        bank = b;
    }
}

static void set_bank_A0BF_ATMAX1024(int b)
{
    if (b != bank) {
        if (b >= 0x80)
            CartA0BF_Disable();
        else {
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image + b * 0x2000);
        }
        bank = b;
    }
}

static void set_bank_80BF(int b)
{
    if (b != bank) {
        if (b & 0x80) {
            Cart809F_Disable();
            CartA0BF_Disable();
        }
        else {
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0xbfff, cart_image + b * 0x4000);
        }
        bank = b;
    }
}

/* an access (read or write) to D500-D5FF area */
static void CART_Access(UWORD addr)
{
    int b = bank;
    switch (myCart.type) 
    {
    case CART_OSS_16:
        if (addr & 0x08)
            b = -1;
        else
            switch (addr & 0x07) {
            case 0x00:
            case 0x01:
                b = 0;
                break;
            case 0x03:
            case 0x07:
                b = 1;
                break;
            case 0x04:
            case 0x05:
                b = 2;
                break;
            /* case 0x02:
            case 0x06: */
            default:
                break;
            }
        set_bank_A0AF(b, 0x3000);
        break;
    case CART_DB_32:
        set_bank_809F(addr & 0x03, 0x6000);
        break;
    case CART_WILL_64:
        set_bank_A0BF_WILL64(addr);
        break;
    case CART_WILL_32:
        set_bank_A0BF_WILL32(addr);
        break;
    case CART_EXP_64:
        if ((addr & 0xf0) == 0x70)
            set_bank_A0BF(addr);
        break;
    case CART_DIAMOND_64:
        if ((addr & 0xf0) == 0xd0)
            set_bank_A0BF(addr);
        break;
    case CART_SDX_64:
        if ((addr & 0xf0) == 0xe0)
            set_bank_A0BF(addr);
        break;
    case CART_OSS2_16:
        switch (addr & 0x09) {
        case 0x00:
            b = 1;
            break;
        case 0x01:
            b = 3;
            break;
        case 0x08:
            b = -1;
            break;
        case 0x09:
            b = 2;
            break;
        }
        set_bank_A0AF(b, 0x0000);
        break;
    case CART_PHOENIX_8:
        CartA0BF_Disable();
        break;
    case CART_BLIZZARD_16:
        Cart809F_Disable();
        CartA0BF_Disable();
        break;
    case CART_ATMAX_128:
        set_bank_A0BF_ATMAX128(addr & 0xff);
        break;
    case CART_ATMAX_1024:
        set_bank_A0BF_ATMAX1024(addr & 0xff);
        break;
    default:
        break;
    }
}

/* a read from D500-D5FF area */
UBYTE CART_GetByte(UWORD addr)
{
    if (rtime_enabled && (addr == 0xd5b8 || addr == 0xd5b9))
        return RTIME_GetByte();
    CART_Access(addr);
    return 0xff;
}

/* a write to D500-D5FF area */
void CART_PutByte(UWORD addr, UBYTE byte)
{
    if (rtime_enabled && (addr == 0xd5b8 || addr == 0xd5b9)) {
        RTIME_PutByte(byte);
        return;
    }
    switch (myCart.type) 
    {
    case CART_XEGS_32:
        set_bank_809F(byte & 0x03, 0x6000);
        break;
    case CART_XEGS_64:
        set_bank_809F(byte & 0x07, 0xe000);
        break;
    case CART_XEGS_128:
        set_bank_809F(byte & 0x0f, 0x1e000);
        break;
    case CART_XEGS_256:
        set_bank_809F(byte & 0x1f, 0x3e000);
        break;
    case CART_XEGS_512:
        set_bank_809F(byte & 0x3f, 0x7e000);
        break;
    case CART_XEGS_1024:
        set_bank_809F(byte & 0x7f, 0xfe000);
        break;
    case CART_ATRAX_128:
        if (byte & 0x80) {
            if (bank >= 0) {
                CartA0BF_Disable();
                bank = -1;
            }
        }
        else {
            int b = byte & 0xf;
            if (b != bank) {
                CartA0BF_Enable();
                CopyROM(0xa000, 0xbfff, cart_image + b * 0x2000);
                bank = b;
            }
        }
        break;
    case CART_MEGA_16:
        set_bank_80BF(byte & 0x80);
        break;
    case CART_MEGA_32:
        set_bank_80BF(byte & 0x81);
        break;
    case CART_MEGA_64:
        set_bank_80BF(byte & 0x83);
        break;
    case CART_MEGA_128:
        set_bank_80BF(byte & 0x87);
        break;
    case CART_MEGA_256:
        set_bank_80BF(byte & 0x8f);
        break;
    case CART_MEGA_512:
        set_bank_80BF(byte & 0x9f);
        break;
    case CART_MEGA_1024:
        set_bank_80BF(byte & 0xbf);
        break;
    case CART_SWXEGS_32:
        set_bank_809F(byte & 0x83, 0x6000);
        break;
    case CART_SWXEGS_64:
        set_bank_809F(byte & 0x87, 0xe000);
        break;
    case CART_SWXEGS_128:
        set_bank_809F(byte & 0x8f, 0x1e000);
        break;
    case CART_SWXEGS_256:
        set_bank_809F(byte & 0x9f, 0x3e000);
        break;
    case CART_SWXEGS_512:
        set_bank_809F(byte & 0xbf, 0x7e000);
        break;
    case CART_SWXEGS_1024:
        set_bank_809F(byte, 0xfe000);
        break;
    default:
        CART_Access(addr);
        break;
    }
}

/* special support of Bounty Bob on Atari5200 */
void CART_BountyBob1(UWORD addr)
{
    if (machine_type == MACHINE_5200) {
        if (addr >= 0x4ff6 && addr <= 0x4ff9) {
            addr -= 0x4ff6;
            CopyROM(0x4000, 0x4fff, cart_image + addr * 0x1000);
        }
    } else {
        if (addr >= 0x8ff6 && addr <= 0x8ff9) {
            addr -= 0x8ff6;
            CopyROM(0x8000, 0x8fff, cart_image + addr * 0x1000);
        }
    }
}

void CART_BountyBob2(UWORD addr)
{
    if (machine_type == MACHINE_5200) {
        if (addr >= 0x5ff6 && addr <= 0x5ff9) {
            addr -= 0x5ff6;
            CopyROM(0x5000, 0x5fff, cart_image + 0x4000 + addr * 0x1000);
        }
    }
    else {
        if (addr >= 0x9ff6 && addr <= 0x9ff9) {
            addr -= 0x9ff6;
            CopyROM(0x9000, 0x9fff, cart_image + 0x4000 + addr * 0x1000);
        }
    }
}

#ifdef PAGED_ATTRIB
UBYTE BountyBob1_GetByte(UWORD addr)
{
    if (machine_type == MACHINE_5200) {
        if (addr >= 0x4ff6 && addr <= 0x4ff9) {
            CART_BountyBob1(addr);
            return 0;
        }
    } else {
        if (addr >= 0x8ff6 && addr <= 0x8ff9) {
            CART_BountyBob1(addr);
            return 0;
        }
    }
    return dGetByte(addr);
}

UBYTE BountyBob2_GetByte(UWORD addr)
{
    if (machine_type == MACHINE_5200) {
        if (addr >= 0x5ff6 && addr <= 0x5ff9) {
            CART_BountyBob2(addr);
            return 0;
        }
    } else {
        if (addr >= 0x9ff6 && addr <= 0x9ff9) {
            CART_BountyBob2(addr);
            return 0;
        }
    }
    return dGetByte(addr);
}

void BountyBob1_PutByte(UWORD addr, UBYTE value)
{
    if (machine_type == MACHINE_5200) {
        if (addr >= 0x4ff6 && addr <= 0x4ff9) {
            CART_BountyBob1(addr);
        }
    } else {
        if (addr >= 0x8ff6 && addr <= 0x8ff9) {
            CART_BountyBob1(addr);
        }
    }
}

void BountyBob2_PutByte(UWORD addr, UBYTE value)
{
    if (machine_type == MACHINE_5200) {
        if (addr >= 0x5ff6 && addr <= 0x5ff9) {
            CART_BountyBob2(addr);
        }
    } else {
        if (addr >= 0x9ff6 && addr <= 0x9ff9) {
            CART_BountyBob2(addr);
        }
    }
}
#endif

int CART_Checksum(const UBYTE *image, int nbytes)
{
    int checksum = 0;
    while (nbytes > 0) {
        checksum += *image++;
        nbytes--;
    }
    return checksum;
}

int CART_Insert(const char *filename) {
#ifdef NOCASH
  char sz[64]; sprintf(sz,"CART_Insert %s %08x\n",filename,cart_image);nocashMessage(sz);
#endif

    FILE *fp;
    int len;
    int type;
    UBYTE header[16];

    /* remove currently inserted cart */
    CART_Remove();

    /* open file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        return CART_CANT_OPEN;
  }
    /* check file length */
    len = Util_flen(fp);
    Util_rewind(fp);

    /* Save Filename for state save */
    strcpy(cart_filename, filename);

    /* if full kilobytes, assume it is raw image */
    if ((len & 0x3ff) == 0) {
#ifdef NOCASH
    nocashMessage("raw image detected\n");
#endif  
        /* alloc memory and read data */
        cart_image = (UBYTE *) Util_malloc(len);
        fread(cart_image, 1, len, fp);
        fclose(fp);
        /* find cart type */

        myCart.type = CART_NONE;
        myCart.control = CTRL_JOY;
        int len_kb = len >> 10; /* number of kilobytes */
        for (type = 1; type <= CART_LAST_SUPPORTED; type++)
        {
            if (cart_kb[type] == len_kb) 
            {
                if (myCart.type == CART_NONE) 
                {
                    myCart.type = type;
                    break;
                }
            }
        }
        
        char md5[33];
        hash_Compute(cart_image, len, (byte*)md5);
        dsPrintValue(0,23,0,md5);
        
        int idx=0;
        while (cart_table[idx].type != CART_NONE)
        {
            if (strncasecmp(cart_table[idx].md5, md5,32) == 0)
            {
                memcpy(&myCart, &cart_table[idx], sizeof(myCart));
                break;   
            }
            idx++;
        }
        
        if (myCart.type != CART_NONE) 
        {
            CART_Start();
#ifdef NOCASH      
      nocashMessage("found cart type");
#endif      
            return 0;   
        }
        free(cart_image);
        cart_image = NULL;
        return CART_BAD_FORMAT;
    }
    /* if not full kilobytes, assume it is CART file */
    fread(header, 1, 16, fp);
    if ((header[0] == 'C') &&
        (header[1] == 'A') &&
        (header[2] == 'R') &&
        (header[3] == 'T')) {
        type = (header[4] << 24) |
            (header[5] << 16) |
            (header[6] << 8) |
            header[7];
    if (CART_IsFor5200(type)) {fclose(fp); return CART_BAD_FORMAT; }
        if (type >= 1 && type <= CART_LAST_SUPPORTED) {
            int checksum;
            len = cart_kb[type] << 10;
            /* alloc memory and read data */
            cart_image = (UBYTE *) Util_malloc(len);
            fread(cart_image, 1, len, fp);
            fclose(fp);
            checksum = (header[8] << 24) |
                (header[9] << 16) |
                (header[10] << 8) |
                header[11];
            myCart.type = type;
            CART_Start();
      {
        int checksum2 = CART_Checksum(cart_image, len);
              int error = (checksum == checksum2) ? 0 : CART_BAD_CHECKSUM;
        return error;
      }
        }
    }
    fclose(fp);
    return CART_BAD_FORMAT;
}

void CART_Remove(void) {
#ifdef NOCASH
    char sz[64];sprintf(sz,"CART_Remove %08x\n",cart_image);nocashMessage(sz);
#endif
    myCart.type = CART_NONE;
    if (cart_image != NULL) {
#ifdef NOCASH
    nocashMessage("free cart image\n");
#endif
        free(cart_image);
        cart_image = NULL;
    }
    CART_Start();
}

void CART_Start(void) {
    if (machine_type == MACHINE_5200) {
        SetROM(0x4ff6, 0x4ff9);     /* disable Bounty Bob bank switching */
        SetROM(0x5ff6, 0x5ff9);
        switch (myCart.type) {
        case CART_5200_32:
#ifdef NOCASH
      nocashMessage("patch CART_5200_32");
#endif      
            CopyROM(0x4000, 0xbfff, cart_image);
            break;
        case CART_5200_EE_16:
#ifdef NOCASH
      nocashMessage("patch CART_5200_EE_16");
#endif      
            CopyROM(0x4000, 0x5fff, cart_image);
            CopyROM(0x6000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0x2000);
            break;
        case CART_5200_40:
#ifdef NOCASH
      nocashMessage("patch CART_5200_40");
#endif      
            CopyROM(0x4000, 0x4fff, cart_image);
            CopyROM(0x5000, 0x5fff, cart_image + 0x4000);
            CopyROM(0x8000, 0x9fff, cart_image + 0x8000);
            CopyROM(0xa000, 0xbfff, cart_image + 0x8000);
#ifndef PAGED_ATTRIB
            SetHARDWARE(0x4ff6, 0x4ff9);
            SetHARDWARE(0x5ff6, 0x5ff9);
#else
            readmap[0x4f] = BountyBob1_GetByte;
            readmap[0x5f] = BountyBob2_GetByte;
            writemap[0x4f] = BountyBob1_PutByte;
            writemap[0x5f] = BountyBob2_PutByte;
#endif
            break;
        case CART_5200_NS_16:
#ifdef NOCASH
      nocashMessage("patch CART_5200_NS_16");
#endif      
            CopyROM(0x8000, 0xbfff, cart_image);
            break;
        case CART_5200_8:
#ifdef NOCASH
      nocashMessage("patch CART_5200_8");
#endif      
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image);
            break;
        case CART_5200_4:
#ifdef NOCASH
      nocashMessage("patch CART_5200_4");
#endif      
            CopyROM(0x8000, 0x8fff, cart_image);
            CopyROM(0x9000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xafff, cart_image);
            CopyROM(0xb000, 0xbfff, cart_image);
            break;
        default:
#ifdef NOCASH
      nocashMessage("patch default");
#endif      
            /* clear cartridge area so the 5200 will crash */
            dFillMem(0x4000, 0, 0x8000);
            break;
        }
    }
    else {
        switch (myCart.type) {
        case CART_STD_8:
        case CART_PHOENIX_8:
            Cart809F_Disable();
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image);
            break;
        case CART_STD_16:
        case CART_BLIZZARD_16:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0xbfff, cart_image);
            break;
        case CART_OSS_16:
            Cart809F_Disable();
            CartA0BF_Enable();
            CopyROM(0xa000, 0xafff, cart_image);
            CopyROM(0xb000, 0xbfff, cart_image + 0x3000);
            bank = 0;
            break;
        case CART_DB_32:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0x6000);
            bank = 0;
            break;
        case CART_WILL_64:
        case CART_WILL_32:
        case CART_EXP_64:
        case CART_DIAMOND_64:
        case CART_SDX_64:
            Cart809F_Disable();
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image);
            bank = 0;
            break;
        case CART_XEGS_32:
        case CART_SWXEGS_32:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0x6000);
            bank = 0;
            break;
        case CART_XEGS_64:
        case CART_SWXEGS_64:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0xe000);
            bank = 0;
            break;
        case CART_XEGS_128:
        case CART_SWXEGS_128:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0x1e000);
            bank = 0;
            break;
        case CART_XEGS_256:
        case CART_SWXEGS_256:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0x3e000);
            bank = 0;
            break;
        case CART_XEGS_512:
        case CART_SWXEGS_512:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0x7e000);
            bank = 0;
            break;
        case CART_XEGS_1024:
        case CART_SWXEGS_1024:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x9fff, cart_image);
            CopyROM(0xa000, 0xbfff, cart_image + 0xfe000);
            bank = 0;
            break;
        case CART_OSS2_16:
            Cart809F_Disable();
            CartA0BF_Enable();
            CopyROM(0xa000, 0xafff, cart_image + 0x1000);
            CopyROM(0xb000, 0xbfff, cart_image);
            bank = 0;
            break;
        case CART_ATRAX_128:
            Cart809F_Disable();
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image);
            bank = 0;
            break;
        case CART_BBSB_40:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0x8fff, cart_image);
            CopyROM(0x9000, 0x9fff, cart_image + 0x4000);
            CopyROM(0xa000, 0xbfff, cart_image + 0x8000);
#ifndef PAGED_ATTRIB
            SetHARDWARE(0x8ff6, 0x8ff9);
            SetHARDWARE(0x9ff6, 0x9ff9);
#else
            readmap[0x8f] = BountyBob1_GetByte;
            readmap[0x9f] = BountyBob2_GetByte;
            writemap[0x8f] = BountyBob1_PutByte;
            writemap[0x9f] = BountyBob2_PutByte;
#endif
            break;
        case CART_RIGHT_8:
            if (machine_type == MACHINE_OSA || machine_type == MACHINE_OSB) {
                Cart809F_Enable();
                CopyROM(0x8000, 0x9fff, cart_image);
                if ((!disable_basic || loading_basic) && have_basic) {
                    CartA0BF_Enable();
                    CopyROM(0xa000, 0xbfff, atari_basic);
                    break;
                }
                CartA0BF_Disable();
                break;
            }
            /* there's no right slot in XL/XE */
            Cart809F_Disable();
            CartA0BF_Disable();
            break;
        case CART_MEGA_16:
        case CART_MEGA_32:
        case CART_MEGA_64:
        case CART_MEGA_128:
        case CART_MEGA_256:
        case CART_MEGA_512:
        case CART_MEGA_1024:
            Cart809F_Enable();
            CartA0BF_Enable();
            CopyROM(0x8000, 0xbfff, cart_image);
            bank = 0;
            break;
        case CART_ATMAX_128:
            Cart809F_Disable();
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image);
            bank = 0;
            break;
        case CART_ATMAX_1024:
            Cart809F_Disable();
            CartA0BF_Enable();
            CopyROM(0xa000, 0xbfff, cart_image + 0xfe000);
            bank = 0x7f;
            break;
        default:
            Cart809F_Disable();
            if ((machine_type == MACHINE_OSA || machine_type == MACHINE_OSB)
             && (!disable_basic || loading_basic) && have_basic) {
                CartA0BF_Enable();
                CopyROM(0xa000, 0xbfff, atari_basic);
                break;
            }
            CartA0BF_Disable();
            break;
        }
    }
}

#ifndef BASIC

void CARTStateRead(void)
{
    int savedCartType = CART_NONE;

    /* Read the cart type from the file.  If there is no cart type, becaused we have
       reached the end of the file, this will just default to CART_NONE */
    ReadINT(&savedCartType, 1);
    if (savedCartType != CART_NONE) {
        char filename[FILENAME_MAX];
        ReadFNAME(filename);
        if (filename[0]) {
            /* Insert the cartridge... */
            if (CART_Insert(filename) >= 0) {
                /* And set the type to the saved type, in case it was a raw cartridge image */
                myCart.type = savedCartType;
                CART_Start();
            }
        }
    }
}

void CARTStateSave(void)
{
    /* Save the cartridge type, or CART_NONE if there isn't one...*/
    SaveINT(&myCart.type, 1);
    if (myCart.type != CART_NONE) {
        SaveFNAME(cart_filename);
    }
}

#endif

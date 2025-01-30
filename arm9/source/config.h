/*
 * config.h contains externs and defines related to A8DS emulator.
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
#ifndef _CONFIG_H
#define _CONFIG_H

// ---------------------------------------------------------------------------
// We write out A8DS.DAT to the /DATA/ directory on the SD card to keep
// a database of user configured settings on a per-game basis. We use 
// the HASH of a game so that we are not reliant on the filename (so even
// if the user renames the file or moves it to another directory, the
// hash will be the same). We picked 2500 as the maximum number of entries
// which works out to a convienent 128K of SD flash memory which is 
// enough for just about anyone and keeps us to using only 4 clusters 
// on the SD card. A reasonable compropmise... 
// ---------------------------------------------------------------------------
#define MAX_GAME_SETTINGS       2500
#define GAME_DATABASE_VERSION   0x0E

struct GameSettings_t
{
    unsigned int game_crc;
    UBYTE slot_used;
    UBYTE machine_type;
    UBYTE tv_type;
    UBYTE cart_type;
    UBYTE basic_enabled;
    UBYTE keyboard_type;
    UBYTE keyMap[8];
    UBYTE skip_frames;
    UBYTE startButtonMap;
    UBYTE selectButtonMap;
    UBYTE auto_fire;
    UBYTE artifacting;
    UBYTE blending;
    UBYTE disk_speedup;
    UBYTE key_click;
    UBYTE dpad_type;
    UBYTE fps_setting;
    UBYTE emulatorText;
    UBYTE alphaBlend;
    UBYTE disk_sound;
    UBYTE analog_speed;
    UBYTE spare0;
    UBYTE spare1;
    UBYTE spare2;
    UBYTE spare3;
    UBYTE spare4;
    UBYTE spare5;
    UBYTE spare6;
    short int xOffset;
    short int yOffset;
    short int xScale;
    short int yScale;
    short int spare7;
    short int spare8;
};

struct GameDatabase_t
{
    UBYTE                       db_version;
    struct GameSettings_t       DefaultGameSettings;
    UBYTE                       default_spare_more[1000];   // Maybe... someday...
    struct GameSettings_t       GameSettings[MAX_GAME_SETTINGS];
    unsigned int                checksum;
};

extern struct GameDatabase_t ConfigDatabase;
extern struct GameSettings_t myConfig;

#define DB_KEY_A    0
#define DB_KEY_B    1
#define DB_KEY_X    2
#define DB_KEY_Y    3
#define DB_KEY_L    4
#define DB_KEY_R    5
#define DB_KEY_STA  6
#define DB_KEY_SEL  7

extern UBYTE option_table;
extern UBYTE disable_basic;
extern UBYTE force_tv_type;
extern UBYTE force_basic_enabled;

#define TV_NTSC     0
#define TV_PAL      1

extern unsigned int last_crc;

extern void InitGameSettings(void);
extern void WriteGameSettings(void);
extern void WriteGlobalSettings(void);
extern void ApplyGameSpecificSettings(void);
extern void ReadGameSettings(void);
extern u8   dsChooseOptions(int bOkayToChangePalette);

#endif // _CONFIG_H

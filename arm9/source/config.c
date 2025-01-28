/*
 * config.c contains configurion handling to map all sorts of options into the emulator.
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
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "main.h"
#include "a8ds.h"

#include "atari.h"
#include "antic.h"
#include "cartridge.h"
#include "input.h"
#include "esc.h"
#include "rtime.h"
#include "emu/pia.h"

#include "clickNoQuit_wav.h"
#include "keyclick_wav.h"
#include "bgBottom.h"
#include "bgTop.h"
#include "bgFileSel.h"
#include "kbd_XL.h"
#include "kbd_XL2.h"
#include "kbd_XE.h"
#include "kbd_400.h"
#include "altirra_os.h"
#include "altirra_basic.h"
#include "config.h"

#pragma pack(1)

struct GameDatabase_t GameDB;
struct GameSettings_t myConfig __attribute__((section(".dtcm")));

UBYTE option_table        = 0;                // We have 2 pages of configuration options - this toggles between them
UBYTE force_tv_type       = 99;               // When selecting a game, the user can force the TV type (NTSC vs PAL)
UBYTE force_basic_enabled = 99;               // When selecting a game, the user can force the use of BASIC

const u16 RAM_SIZES[] = {16, 48, 64, 128, 320, 576, 1088}; // For the various machine_types

// -----------------------------------------------------------------------------------------
// Some basic machine settings... we default to an Atari 800XL with 128K of RAM installed.
// This is a bit of a "custom" machine but is capable of running 95% of all games.
// -----------------------------------------------------------------------------------------
UBYTE disable_basic    = TRUE;

u8 UpgradeConfig(void)
{
    u8 bInitNeeded = true;  // We only handle some upgrades.. default to requiring a full config re-init unless we prove otherwise
    
    return bInitNeeded;
}

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
        // Map one with a default of '1' which may come in handy in the future....
        GameDB.GameSettings[i].spare6 = 1;
    }
    GameDB.default_skip_frames        = (isDSiMode() ? 0:1);  // For older DS models, we skip frames to get full speed...
    GameDB.default_machine_type       = MACHINE_XLXE_128K;    // Default machine is the XL/XE with 128K of RAM
    GameDB.default_blending           = 1;                    // Light Blending
    GameDB.default_alphaBlend         = 0;                    // No Alpha Blend
    GameDB.default_disk_speedup       = 1;                    // Disk is Fast
    GameDB.default_disk_sound         = 0;                    // No SIO sound
    GameDB.default_keyMap[DB_KEY_A]   = 0;                    // Fire button
    GameDB.default_keyMap[DB_KEY_B]   = 0;                    // Fire button
    GameDB.default_keyMap[DB_KEY_X]   = 65;                   // VERTICAL-
    GameDB.default_keyMap[DB_KEY_Y]   = 14;                   // SPACE BAR
    GameDB.default_keyMap[DB_KEY_L]   = 72;                   // Scale X,Y
    GameDB.default_keyMap[DB_KEY_R]   = 71;                   // Offset X,Y
    GameDB.default_keyMap[DB_KEY_STA] = 10;                   // START
    GameDB.default_keyMap[DB_KEY_SEL] = 11;                   // SELECT
    GameDB.db_version = GAME_DATABASE_VERSION;
}


// -------------------------------------------------------------------------------------
// Snap out the A8DS.DAT to the SD card. This is only done when the user asks for it 
// to be written out... either by holding both L/R shoulder buttons on the DS for a
// full half-second or by pressing START while in the configuration area. 
// -------------------------------------------------------------------------------------
void WriteGameSettings(void)
{
    FILE *fp;
    int idx = 0;

    GameDB.db_version = GAME_DATABASE_VERSION;
    // Search through the Game Database to see if we have a match to our game filename....
    for (idx=0; idx < MAX_GAME_SETTINGS; idx++)
    {
        if (GameDB.GameSettings[idx].slot_used == 0) break;
        if (GameDB.GameSettings[idx].game_crc == last_crc) break;
    }

    if (idx < MAX_GAME_SETTINGS)
    {
        GameDB.GameSettings[idx].game_crc           = last_crc;
        GameDB.GameSettings[idx].slot_used          = 1;
        GameDB.GameSettings[idx].machine_type       = myConfig.machine_type;
        GameDB.GameSettings[idx].tv_type            = myConfig.tv_type;
        GameDB.GameSettings[idx].basic_enabled      = myConfig.basic_enabled;
        GameDB.GameSettings[idx].cart_type          = myConfig.cart_type;
        GameDB.GameSettings[idx].keyboard_type      = myConfig.keyboard_type;
        GameDB.GameSettings[idx].fps_setting        = myConfig.fps_setting;        
        GameDB.GameSettings[idx].auto_fire          = myConfig.auto_fire;
        GameDB.GameSettings[idx].skip_frames        = myConfig.skip_frames;
        GameDB.GameSettings[idx].artifacting        = myConfig.artifacting;
        GameDB.GameSettings[idx].xOffset            = myConfig.xOffset;
        GameDB.GameSettings[idx].yOffset            = myConfig.yOffset;
        GameDB.GameSettings[idx].xScale             = myConfig.xScale;
        GameDB.GameSettings[idx].yScale             = myConfig.yScale;
        GameDB.GameSettings[idx].blending           = myConfig.blending;
        GameDB.GameSettings[idx].key_click_disable  = myConfig.key_click_disable;
        GameDB.GameSettings[idx].dpad_type          = myConfig.dpad_type;
        GameDB.GameSettings[idx].disk_speedup       = myConfig.disk_speedup;
        GameDB.GameSettings[idx].disk_sound         = myConfig.disk_sound;
        GameDB.GameSettings[idx].emulatorText       = myConfig.emulatorText;
        GameDB.GameSettings[idx].alphaBlend         = myConfig.alphaBlend;
        GameDB.GameSettings[idx].analog_speed       = myConfig.analog_speed;
        for (int i=0; i<8; i++) GameDB.GameSettings[idx].keyMap[i] = myConfig.keyMap[i];
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
        fp = fopen("/data/A8DS.DAT", "wb+");
        if (fp != NULL)
        {
            fwrite(&GameDB, sizeof(GameDB), 1, fp);
            fclose(fp);
        }
    }
}

// -------------------------------------------------------------------------------------
// Snap out the A8DS.DAT to the SD card. This is only done when the user asks for it 
// to be written out... either by holding both L/R shoulder buttons on the DS for a
// full half-second or by pressing START while in the configuration area. 
// -------------------------------------------------------------------------------------
void WriteGlobalSettings(void)
{
    FILE *fp;

    GameDB.db_version = GAME_DATABASE_VERSION;

    GameDB.default_tv_type            = myConfig.tv_type;
    GameDB.default_machine_type       = myConfig.machine_type;
    GameDB.default_basic_enabled      = myConfig.basic_enabled;
    GameDB.default_keyboard_type      = myConfig.keyboard_type;
    GameDB.default_skip_frames        = myConfig.skip_frames;
    GameDB.default_key_click_disable  = myConfig.key_click_disable;
    GameDB.default_auto_fire          = myConfig.auto_fire;
    GameDB.default_blending           = myConfig.blending;
    GameDB.default_alphaBlend         = myConfig.alphaBlend;
    GameDB.default_disk_speedup       = myConfig.disk_speedup;
    GameDB.default_disk_sound         = myConfig.disk_sound;
    for (int i=0; i<8; i++) GameDB.default_keyMap[i] = myConfig.keyMap[i];
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
    fp = fopen("/data/A8DS.DAT", "wb+");
    if (fp != NULL)
    {
        fwrite(&GameDB, sizeof(GameDB), 1, fp);
        fclose(fp);
    }
}

// ----------------------------------------------------------------------------------
// Read the A8DS.DAT file from the SD card and into memory. If we can't find the
// file or if the file is corrupt, we will write out a blank default database.
// ----------------------------------------------------------------------------------
void ReadGameSettings(void)
{
    FILE *fp;
    u8 bInitNeeded = false;

    fp = fopen("/data/A8DS.DAT", "rb");
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
        
        // If checksum is bad, we must re-init. Can't trust the file...
        if (GameDB.checksum != checksum)
        {
            bInitNeeded = true;
        }
        
        // If the database version is old but checksum is good.... we might be able to update the config
        if ((GameDB.db_version != GAME_DATABASE_VERSION) && (GameDB.checksum == checksum))
        {
            bInitNeeded = UpgradeConfig();  // See if we can upgrade the config database automatically
        }
        
        if (bInitNeeded)
        {
            InitGameSettings();
        }
    }
    else
    {
        InitGameSettings();
    }

    myConfig.machine_type       = GameDB.default_machine_type;
    myConfig.tv_type            = GameDB.default_tv_type;
    myConfig.basic_enabled      = GameDB.default_basic_enabled;
    myConfig.keyboard_type      = GameDB.default_keyboard_type;
    myConfig.blending           = GameDB.default_blending;
    myConfig.alphaBlend         = GameDB.default_alphaBlend;
    myConfig.skip_frames        = GameDB.default_skip_frames;
    myConfig.auto_fire          = GameDB.default_auto_fire;
    myConfig.key_click_disable  = GameDB.default_key_click_disable;
    myConfig.disk_speedup       = GameDB.default_disk_speedup;
    myConfig.disk_sound         = GameDB.default_disk_sound;
    myConfig.emulatorText       = true;
    myConfig.analog_speed       = 0;
    myConfig.fps_setting        = 0;
    myConfig.dpad_type          = 0;
    myConfig.artifacting        = 0;
    
    for (int i=0; i<8; i++) myConfig.keyMap[i] = GameDB.default_keyMap[i];
}

void SetMyConfigDefaults(void)
{
    myConfig.xOffset            = 32;
    myConfig.yOffset            = 24;
    myConfig.xScale             = 256;
    myConfig.yScale             = 256;
    myConfig.artifacting        = 0;
    myConfig.cart_type          = CART_NONE;
    myConfig.emulatorText       = true;
    myConfig.fps_setting        = 0;
    myConfig.dpad_type          = 0;
    myConfig.analog_speed       = 0;
    myConfig.machine_type       = GameDB.default_machine_type;
    myConfig.basic_enabled      = GameDB.default_basic_enabled;
    myConfig.tv_type            = GameDB.default_tv_type;
    myConfig.keyboard_type      = GameDB.default_keyboard_type;
    myConfig.blending           = GameDB.default_blending;
    myConfig.alphaBlend         = GameDB.default_alphaBlend;
    myConfig.skip_frames        = GameDB.default_skip_frames;
    myConfig.auto_fire          = GameDB.default_auto_fire;
    myConfig.key_click_disable  = GameDB.default_key_click_disable;
    myConfig.disk_speedup       = GameDB.default_disk_speedup;
    myConfig.disk_sound         = GameDB.default_disk_sound;
    
    for (int i=0; i<8; i++)  myConfig.keyMap[i] = GameDB.default_keyMap[i];
}

// ---------------------------------------------------------------------------------
// Look for the game by hash in the A8DS.DAT database. If found, we apply the 
// restored game-specific settings. Otherwise we use defaults.
// ---------------------------------------------------------------------------------
void ApplyGameSpecificSettings(void)
{
    int idx = 0;

    // Search through the Game Database to see if we have a match to our game filename....
    for (idx=0; idx < MAX_GAME_SETTINGS; idx++)
    {
        if (GameDB.GameSettings[idx].game_crc == last_crc) break;
    }

    if (idx < MAX_GAME_SETTINGS)    // We found a match in the database... use it!
    {
        myConfig.machine_type       = GameDB.GameSettings[idx].machine_type;
        myConfig.tv_type            = GameDB.GameSettings[idx].tv_type;
        myConfig.basic_enabled      = GameDB.GameSettings[idx].basic_enabled;
        myConfig.cart_type          = GameDB.GameSettings[idx].cart_type;
        myConfig.keyboard_type      = GameDB.GameSettings[idx].keyboard_type;
        
        myConfig.xOffset            = GameDB.GameSettings[idx].xOffset;
        myConfig.yOffset            = GameDB.GameSettings[idx].yOffset;
        myConfig.xScale             = GameDB.GameSettings[idx].xScale;
        myConfig.yScale             = GameDB.GameSettings[idx].yScale;
        myConfig.artifacting        = GameDB.GameSettings[idx].artifacting;
        myConfig.fps_setting        = GameDB.GameSettings[idx].fps_setting;            
        myConfig.auto_fire          = GameDB.GameSettings[idx].auto_fire;
        myConfig.skip_frames        = GameDB.GameSettings[idx].skip_frames;
        myConfig.blending           = GameDB.GameSettings[idx].blending;
        myConfig.key_click_disable  = GameDB.GameSettings[idx].key_click_disable;
        myConfig.dpad_type          = GameDB.GameSettings[idx].dpad_type;
        myConfig.disk_speedup       = GameDB.GameSettings[idx].disk_speedup;
        myConfig.disk_sound         = GameDB.GameSettings[idx].disk_sound;
        myConfig.emulatorText       = GameDB.GameSettings[idx].emulatorText;
        myConfig.alphaBlend         = GameDB.GameSettings[idx].alphaBlend;
        myConfig.analog_speed       = GameDB.GameSettings[idx].analog_speed;
        for (int i=0; i<8; i++)  myConfig.keyMap[i] = GameDB.GameSettings[idx].keyMap[i];
    }
    else // No match. Use defaults for this game...
    {
        SetMyConfigDefaults();
    }
    
    // When the game was selected, the user can override the TV TYPE (PAL vs NTSC)
    if (force_tv_type != 99)
    {
        myConfig.tv_type = force_tv_type;
        if (!bFirstLoad) force_tv_type = 99;
    }    

    // When the game was selected, the user can override the BASIC TYPE (NONE vs some available type)
    if (force_basic_enabled != 99)
    {
        myConfig.basic_enabled = force_basic_enabled;
        if (!bFirstLoad) force_basic_enabled = 99;
    }

    install_os();
 }


// ---------------------------------------------------------------------------
// Write out the A8DS.DAT configuration file to capture the settings for
// each game.
// ---------------------------------------------------------------------------
void dsWriteConfig(void)
{
    dsPrintValue(1,0,0, (char*)"CONFIG SAVE");
    WriteGameSettings();

    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    dsPrintValue(1,0,0, (char*)"           ");
}

// ---------------------------------------------------------------------------
// Write out the A8DS.DAT configuration file to capture the settings for
// each game.
// ---------------------------------------------------------------------------
void dsWriteGlobalConfig(void)
{
    dsPrintValue(1,0,0, (char*)"GLOBAL SAVE");
    WriteGlobalSettings();

    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    dsPrintValue(1,0,0, (char*)"           ");
}

// -----------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off 
// their option choices for the currently running game into the A8DS.DAT
// configuration database. When games are loaded back up, A8DS.DAT is read
// to see if we have a match and the user settings can be restored for the 
// game.
// -----------------------------------------------------------------------------
struct options_t
{
    char *label;
    char *option[120];
    UBYTE *option_val;
    UBYTE option_type;
    UBYTE option_max;
    char *help1;
    char *help2;
    char *help3;
    char *help4;
};

#define OPT_NORMAL  0
#define OPT_KEYSEL  1
#define OPT_NUMERIC 2

#define KEY_MAP_TEXT {"JOY 1 FIRE", "JOY 1 UP", "JOY 1 DOWN", "JOY 1 LEFT", "JOY 1 RIGHT", "JOY 2 FIRE", "JOY 2 UP", "JOY 2 DOWN", "JOY 2 LEFT", "JOY 2 RIGHT",     \
                      "CONSOLE START", "CONSOLE SEL", "CONSOLE OPT", "CONSOLE HELP", "KEY SPACE", "KEY RETURN", "KEY ESC", "KEY BREAK",                             \
                      "KEY A", "KEY B", "KEY C", "KEY D", "KEY E", "KEY F", "KEY G", "KEY H", "KEY I", "KEY J", "KEY K", "KEY L", "KEY M", "KEY N", "KEY O",        \
                      "KEY P", "KEY Q", "KEY R", "KEY S", "KEY T", "KEY U", "KEY V", "KEY W", "KEY X", "KEY Y", "KEY Z", "KEY 0", "KEY 1", "KEY 2", "KEY 3",        \
                      "KEY 4", "KEY 5", "KEY 6", "KEY 7", "KEY 8", "KEY 9", "KEY UP", "KEY DOWN", "KEY LEFT", "KEY RIGHT", "SHIFT", "CONTROL",                      \
                      "KEY SPARE1", "KEY SPARE2", "KEY SPARE3", "VERTICAL+", "VERTICAL++", "VERTICAL-", "VERTICAL--", "HORIZONTAL+", "HORIZONTAL++", "HORIZONTAL-", \
                       "HORIZONTAL--", "OFFSET DPAD", "SCALE DPAD", "ZOOM SCREEN"}

#define CART_TYPES {"00-NONE",       "01-STD8",       "02-STD16",      "03-OSS16-034M", "04-5200 32K",   "05-DB32",       "06-5200 16-EE", "07-NO SUPPORT", "08-WILLIAMS64", "09-EXP64",      \
                    "10-DIAMOND64",  "11-SDX64",      "12-XEGS32",     "13-XEGS64",     "14-XEGS128",    "15-OSS16",      "16-5200 16-NS", "17-ATRAX128",   "18-BOUNTY BOB", "19-5200 8K",    \
                    "20-5200 4K",    "21-RIGHT 8K",   "22-WILLIAMS32", "23-XEGS256",    "24-XEGS512",    "25-XEGS1024",   "26-MEGA16",     "27-MEGA32",     "28-MEGA64",     "29-MEGA128",    \
                    "30-MEGA256",    "31-MEGA512",    "32-MEGA1024",   "33-SWXEGS32",   "34-SWXEGS64",   "35-SWXEGS128",  "36-SWXEGS256",  "37-SWXEGS512",  "38-SWXEGS1024", "39-PHOENIX8",   \
                    "40-BLIZZARD16", "41-ATMAX128",   "42-ATMAX1024",  "43-SDX128",     "44-OSS8",       "45-OSS16-043M", "46-BLIZZARD4",  "47-AST32",      "48-NO SUPPORT", "49-NO SUPPORT", \
                    "50-TURBO64",    "51-TURBO128",   "52-ULTRACART",  "53-LOWBANK_8",  "54-SIC128",     "55-SIC256",     "56-SIC512",     "57-STD2",       "58-STD4",       "59-RIGHT 4K",   \
                    "60-BLIZZARD32", "61-NO SUPPORT", "62-NO SUPPORT", "63-NO SUPPORT", "64-NO SUPPORT", "65-NO SUPPORT", "66-NO SUPPORT", "67-NO SUPPORT", "68-NO SUPPORT", "69-ADAWLIAH32", \
                    "70-ADAWLIAH64", "71-NO SUPPORT", "72-NO SUPPORT", "73-NO SUPPORT", "74-NO SUPPORT", "75-ATMAX-NEW",  "76-WILLIAMS16", "77-NO SUPPORT", "78-NO SUPPORT", "79-NO SUPPORT", \
                    "80-JRC64",      "81-NO SUPPORT", "82-NO SUPPORT", "83-NO SUPPORT", "84-NO SUPPORT", "85-NO SUPPORT", "86-NO SUPPORT", "87-NO SUPPORT", "88-NO SUPPORT", "89-NO SUPPORT", \
                    "90-NO SUPPORT", "91-NO SUPPORT", "92-NO SUPPORT", "93-NO SUPPORT", "94-NO SUPPORT", "95-NO SUPPORT", "96-NO SUPPORT", "97-NO SUPPORT", "98-NO SUPPORT", "99-NO SUPPORT", \
                    "100-NO SUPPORT","101-NO SUPPORT","102-NO SUPPORT","103-NO SUPPORT","104-JATARI8",   "105-JATARI16",  "106-JATARI32",  "107-JATARI64",  "108-JATARI128", "109-JATARI256", \
                    "110-JATARI512", "111-JATARI1024","112-DCART",     "113-NO SUPPORT","114-NO SUPPORT","115-NO SUPPORT"}

#define MACHINE_TYPES {"A5200 16K", "A800 48K", "XLXE 64K",  "XLXE 128K", "320K RAMBO", "576K COMPY", "XLXE 1088K"}

const struct options_t Option_Table[2][20] =
{
    // Page 1
    {
        {"MACHINE TYPE",MACHINE_TYPES,                                      &myConfig.machine_type,         OPT_NORMAL, 7,   "XLXE 64K/128K FOR ",   "GAMES. USE A800   ",  "OLDER GAMES FOR   ",  "COMPATIBILITY.    "},
        {"TV TYPE",     {"NTSC",        "PAL"},                             &myConfig.tv_type,              OPT_NORMAL, 2,   "NTSC=60 FPS       ",   "WITH 262 SCANLINES",  "PAL=50 FPS        ",  "WITH 312 SCANLINES"},
        {"BASIC",       {"DISABLED",    "ENABLED"},                         &myConfig.basic_enabled,        OPT_NORMAL, 2,   "NORMALLY DISABLED ",   "EXCEPT FOR BASIC  ",  "GAMES THAT REQUIRE",  "THE CART INSERTED "},
        {"CART TYPE",   CART_TYPES,                                         &myConfig.cart_type,            OPT_NORMAL,116,  "ROM FILES DONT    ",   "ALWAYS AUTODETECT ",  "SO YOU CAN SET THE",  "CARTRIDGE TYPE    "},
        {"SKIP FRAMES", {"NO",          "MODERATE",     "AGGRESSIVE"},      &myConfig.skip_frames,          OPT_NORMAL, 3,   "OFF NORMALLY AS   ",   "SOME GAMES CAN    ",  "GLITCH WHEN SET   ",  "TO FRAMESKIP      "},
        {"FPS SETTING", {"OFF",         "ON", "ON-TURBO"},                  &myConfig.fps_setting,          OPT_NORMAL, 3,   "SHOW FPS ON MAIN  ",   "DISPLAY. OPTIONALY",  "RUN IN TURBO MODE ",  "FAST AS POSSIBLE  "},
        {"ARTIFACTING", {"OFF",         "1:BROWN/BLUE", "2:BLUE/BROWN",
                                        "3:RED/GREEN","4:GREEN/RED"},       &myConfig.artifacting,          OPT_NORMAL, 5,   "A FEW HIRES GAMES ",   "NEED ARTIFACING   ",  "TO LOOK RIGHT     ",  "OTHERWISE SET OFF "},
        {"SCREEN BLUR", {"NONE",        "LIGHT", "HEAVY"},                  &myConfig.blending,             OPT_NORMAL, 3,   "NORMALLY LIGHT    ",   "BLUR TO HELP WITH ",  "SCREEN SCALING    ",  "                  "},
        {"ALPHA BLEND", {"OFF",         "ON"},                              &myConfig.alphaBlend,           OPT_NORMAL, 2,   "TURN THIS ON TO   ",   "BLEND FRAMES. THIS",  "MAKES THE SCREEN  ",  "BRIGHTER ON NON-XL"},
        {"KEY CLICK",   {"ON",          "OFF"},                             &myConfig.key_click_disable,    OPT_NORMAL, 2,   "NORMALLY ON       ",   "CAN BE USED TO    ",  "SILENCE KEY CLICKS",  "FOR KEYBOARD USE  "},
        {"DISK SPEED",  {"ACCURATE",    "FAST"},                            &myConfig.disk_speedup,         OPT_NORMAL, 2,   "NORMALLY FAST IS  ",   "DESIRED TO SPEED  ",  "UP FLOPPY DISK.   ",  "ACCURATE FOR SOME "},
        {"DISK SOUND",  {"DISABLED",    "ENABLED"},                         &myConfig.disk_sound,           OPT_NORMAL, 2,   "ENABLE FOR SOUND  ",   "EFFECTS ON SIO    ",  "ACCESS. OTHERWISE ",  "DISABLED.         "},
        {"EMULATOR TXT",{"OFF",         "ON"},                              &myConfig.emulatorText,         OPT_NORMAL, 2,   "NORMALLY ON       ",   "CAN BE USED TO    ",  "DISABLE FILENAME  ",  "INFO ON MAIN SCRN "},
        {"KEYBOARD",    {"800XL STYLE1","800XL STYLE2", "400 STYLE", 
                         "130XE STYLE", "ALPHANUMERIC", "STAR RAIDER"},     &myConfig.keyboard_type,        OPT_NORMAL, 6,   "CHOOSE THE STYLE  ",   "THAT BEST SUITS   ",  "YOUR TASTES.      ",  "                  "},
        {NULL,          {"",            ""},                                NULL,                           OPT_NORMAL, 2,   "HELP1             ",   "HELP2             ",  "HELP3             ",  "HELP4             "}
    },
    // Page 2
    {
        {"A BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[0],            OPT_KEYSEL, 72,  "SET THE A KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"B BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[1],            OPT_KEYSEL, 72,  "SET THE B KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"X BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[2],            OPT_KEYSEL, 72,  "SET THE X KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"Y BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[3],            OPT_KEYSEL, 72,  "SET THE Y KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"L BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[4],            OPT_KEYSEL, 72,  "SET THE L KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"R BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[5],            OPT_KEYSEL, 72,  "SET THE R KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"START BTN",   KEY_MAP_TEXT,                                       &myConfig.keyMap[6],            OPT_KEYSEL, 72,  "SET START KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"SELECT BTN",  KEY_MAP_TEXT,                                       &myConfig.keyMap[7],            OPT_KEYSEL, 72,  "SET SELECT KEY TO ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"D-PAD",       {"JOY 1", "JOY 2", "DIAGONALS", "CURSORS"},         &myConfig.dpad_type,            OPT_NORMAL, 4,   "CHOOSE HOW THE    ",   "JOYSTICK OPERATES ",  "CAN SWAP JOY1 AND ",  "JOY2 OR MAP CURSOR"},    
        {"AUTOFIRE",    {"OFF",         "SLOW",   "MED",  "FAST"},          &myConfig.auto_fire,            OPT_NORMAL, 4,   "TOGGLE AUTOFIRE   ",   "SLOW = 4x/SEC     ",  "MED  = 8x/SEC     ",  "FAST = 15x/SEC    "},
        {"5200 ANALOG",  {"FAST", "MEDIUM", "SLOW"},                         &myConfig.analog_speed,         OPT_NORMAL, 3,   "FOR THE A5200 THIS",   "REPRESENTS SPEED  ",  "OF THE ANALOG JOY ",  "STICK. EXPERIMENT."},
        {"X OFFSET",    {"XX"},                                     (UBYTE*)&myConfig.xOffset,              OPT_NUMERIC,0,   "SET SCREEN OFFSET ",   "                  ",  "                  ",  "                  "},
        {"Y OFFSET",    {"XX"},                                     (UBYTE*)&myConfig.yOffset,              OPT_NUMERIC,0,   "SET SCREEN OFFSET ",   "                  ",  "                  ",  "                  "},
        {"X SCALE",     {"XX"},                                     (UBYTE*)&myConfig.xScale,               OPT_NUMERIC,0,   "SET SCREEN SCALE  ",   "                  ",  "                  ",  "                  "},
        {"Y SCALE",     {"XX"},                                     (UBYTE*)&myConfig.yScale,               OPT_NUMERIC,0,   "SET SCREEN SCALE  ",   "                  ",  "                  ",  "                  "},
        
        {NULL,          {"",            ""},                                NULL,                           OPT_NORMAL, 2,   "HELP1             ",   "HELP2             ",  "HELP3             ",  "HELP4             "}
    }
};

u8 display_options_list(bool bFullDisplay)
{
    static char strBuf[35];
    short int len=0;
    
    dsPrintValue(1,21, 0, (char *)"                              ");
    if (bFullDisplay)
    {
        while (true)
        {
            if (Option_Table[option_table][len].option_type == OPT_NUMERIC)
            {
                siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][len].label, *((short int*)Option_Table[option_table][len].option_val));
            }
            else
            {
                siprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][len].label, Option_Table[option_table][len].option[*(Option_Table[option_table][len].option_val)]);
            }
            dsPrintValue(1,6+len, (len==0 ? 1:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<16; i++) 
        {
            dsPrintValue(1,6+i, 0, (char *)"                               ");
        }
    }

    dsPrintValue(0,23, 0, (char *)"B=EXIT X=MORE START=SAVE SEL=DEF");
    return len;    
}

void ShowConfigBackground(void)
{
    decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
}

struct GameSettings_t lastConfig;

// -----------------------------------------------------------------------------
// Allows the user to move the cursor up and down through the various table 
// enties  above to select options for the game they wish to play. 
// -----------------------------------------------------------------------------
u8 dsChooseOptions(int bOkayToChangePalette)
{
    static int last_pal=-1;
    static int last_art=-1;
    static char strBuf[64];
    short int optionHighlighted;
    short int idx;
    u8 bDone=false;
    short unsigned int keys_pressed;
    short unsigned int last_keys_pressed = 999;
    short unsigned int key_repeat_counter = 0;
    u8 reset_system = 0;

    u8 last_tv_type       = myConfig.tv_type;
    u8 last_mach_type     = myConfig.machine_type;
    u8 last_cart_type     = myConfig.cart_type;
    u8 last_basic_enabled = myConfig.basic_enabled;
    
    memcpy(&lastConfig, &myConfig, sizeof(lastConfig)); // So we can detect if config changed...
    
    option_table = 0;
    
    // Show the background...
    ShowConfigBackground();

    idx=display_options_list(true);
    optionHighlighted = 0;
    while (!bDone)
    {
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    siprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,6+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    siprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,6+optionHighlighted,1, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    siprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,6+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    siprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,6+optionHighlighted,1, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Next Choice 
            {
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    short int *val=(short int *)Option_Table[option_table][optionHighlighted].option_val;
                    (*val)++;
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) + 1) % Option_Table[option_table][optionHighlighted].option_max;
                    siprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,6+optionHighlighted,1, strBuf);
                dsPrintValue(0,0,0,"           ");
            }
            if (keysCurrent() & KEY_LEFT)  // Previous Choice 
            {
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    short int *val=(short int *)Option_Table[option_table][optionHighlighted].option_val;
                    (*val)--;
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    if (*(Option_Table[option_table][optionHighlighted].option_val) > 0) 
                        *(Option_Table[option_table][optionHighlighted].option_val) -= 1;
                    else
                         *(Option_Table[option_table][optionHighlighted].option_val) = Option_Table[option_table][optionHighlighted].option_max - 1;
                    siprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,6+optionHighlighted,1, strBuf);
                dsPrintValue(0,0,0,"           ");
            }            
            if (keysCurrent() & KEY_START)  // Save Options
            {
                if (bOkayToChangePalette)   // This lets us know that a game is selected...
                {
                    dsWriteConfig();
                    memcpy(&lastConfig, &myConfig, sizeof(lastConfig)); // So we can detect if config changed...
                }
                else
                {
                    dsWriteGlobalConfig();
                }
            }
            if (keysCurrent() & KEY_SELECT)  // Use Default Options
            {
                if (bOkayToChangePalette)   // This lets us know that a game is selected...
                {
                    SetMyConfigDefaults();
                    idx=display_options_list(true);
                    optionHighlighted = 0;
                } else dsWriteGlobalConfig();
            }
            if (keysCurrent() & KEY_X)  // Toggle options
            {
                option_table = (option_table + 1) % 2;
                idx=display_options_list(true);
                optionHighlighted = 0;
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
            }
            if (keysCurrent() & KEY_B)  // Exit options
            {
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
                WAITVBL;WAITVBL;
                
                if (bOkayToChangePalette)
                {
                    if ((last_tv_type != myConfig.tv_type) || (last_mach_type != myConfig.machine_type) || (last_cart_type != myConfig.cart_type) || (last_basic_enabled != myConfig.basic_enabled))
                    {
                        if (dsQuery("YOU'VE CHANGED A FUNDAMENTAL", "SETTING. SAVE AND RESET?"))
                        {
                            if (memcmp(&lastConfig, &myConfig, sizeof(lastConfig)) != 0)
                            {
                                dsWriteConfig();
                            }                            
                            install_os();
                            reset_system = 1;
                        } 
                        else
                        {
                             // Show the background...
                             ShowConfigBackground();
                             idx = display_options_list(true);
                             optionHighlighted = 0;
                             continue;    // Stay in the config loop...
                        }
                    }
                }
                break;
            }

            // In case the Artifacting global changed....
            if (last_art != myConfig.artifacting)
            {
                last_art = myConfig.artifacting;
                ANTIC_UpdateArtifacting();
            }
            
            // In case the TV Type (NTSC vs PAL) global changed....
            if (last_pal != myConfig.tv_type)
            {
                last_pal = myConfig.tv_type;
                if (bOkayToChangePalette) dsSetAtariPalette();
            }

            dsPrintValue(14,0,0,Option_Table[option_table][optionHighlighted].help1);
            dsPrintValue(14,1,0,Option_Table[option_table][optionHighlighted].help2);
            dsPrintValue(14,2,0,Option_Table[option_table][optionHighlighted].help3);
            dsPrintValue(14,3,0,Option_Table[option_table][optionHighlighted].help4);
            
            key_repeat_counter = 0;
        }
        else
        {
            if (++key_repeat_counter >= 10) last_keys_pressed = 999;
        }
        swiWaitForVBlank();
    }

    // Restore original bottom graphic
    decompress(bgBottomTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgBottomMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgBottomPal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

    dsShowRomInfo();
    
    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }

    return reset_system;
}


// End of file

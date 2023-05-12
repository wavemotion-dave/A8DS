/*
 * config.c contains configurion handling.
 * 
 * A8DS - Atari 8-bit Emulator designed to run on the Nintendo DS/DSi is
 * Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)

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
#include "kbd_XL.h"
#include "kbd_XL2.h"
#include "kbd_XE.h"
#include "kbd_400.h"
#include "altirra_os.h"
#include "altirra_basic.h"
#include "config.h"

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

struct GameDatabase_t GameDB;
struct GameSettings_t myConfig __attribute__((section(".dtcm")));

UBYTE bAtariOS=false;                         // Real Atari XL BIOS is OFF by default
UBYTE bAtariOSB=false;                        // Real Atari OSB BIOS is OFF by default
UBYTE bAtariBASIC=false;                      // Real Atari Basic Rev C is OFF by default

UBYTE full_speed          = false;            // default is to run at normal speed
UBYTE cart_type           = CART_NONE;        // 
UBYTE option_table        = 0;

UWORD force_tv_type       = 99;
UWORD force_basic_type    = 99;

// ----------------------------------------------------------------------------------
// Some basic machine settings... we default to an XEGS with 128K of RAM installed.
// This is a bit of a "custom" machine but is capable of running 98% of all games.
// ----------------------------------------------------------------------------------
UBYTE machine_type     = MACHINE_XLXE;
UBYTE disable_basic    = TRUE;
UBYTE file_type        = AFILE_ERROR;

UWORD  ram_size         = RAM_128K;        // We only allow RAM_128K or RAM_320_RAMBO and for backwards compatibility RAM_48K

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
    GameDB.default_skip_frames = (isDSiMode() ? 0:1);   // For older DS models, we skip frames to get full speed...
    GameDB.default_os_type = OS_ALTIRRA_XL;
    GameDB.default_blending = 6;
    GameDB.default_keyMap[DB_KEY_A] = 0;
    GameDB.default_keyMap[DB_KEY_B] = 0;
    GameDB.default_keyMap[DB_KEY_X] = 8;
    GameDB.default_keyMap[DB_KEY_Y] = 9;
    GameDB.default_keyMap[DB_KEY_L] = 64;
    GameDB.default_keyMap[DB_KEY_R] = 63;
    GameDB.default_keyMap[DB_KEY_STA] = 5;
    GameDB.default_keyMap[DB_KEY_SEL] = 6;
    GameDB.db_version = GAME_DATABASE_VERSION;
}

// -------------------------------------------------------------------------------------
// Snap out the XEGS.DAT to the SD card. This is only done when the user asks for it 
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
        if (memcmp (GameDB.GameSettings[idx].game_hash, last_hash, 32) == 0) break;
    }

    if (idx < MAX_GAME_SETTINGS)
    {
        memcpy(GameDB.GameSettings[idx].game_hash, last_hash, 32);
        GameDB.GameSettings[idx].slot_used          = 1;
        GameDB.GameSettings[idx].tv_type            = myConfig.tv_type;
        GameDB.GameSettings[idx].palette_type       = myConfig.palette_type;
        GameDB.GameSettings[idx].os_type            = myConfig.os_type;
        GameDB.GameSettings[idx].basic_type         = myConfig.basic_type;
        GameDB.GameSettings[idx].auto_fire          = myConfig.auto_fire;
        GameDB.GameSettings[idx].skip_frames        = myConfig.skip_frames;
        GameDB.GameSettings[idx].ram_type           = myConfig.ram_type;
        GameDB.GameSettings[idx].artifacting        = myConfig.artifacting;
        GameDB.GameSettings[idx].xOffset            = myConfig.xOffset;
        GameDB.GameSettings[idx].yOffset            = myConfig.yOffset;
        GameDB.GameSettings[idx].xScale             = myConfig.xScale;
        GameDB.GameSettings[idx].yScale             = myConfig.yScale;
        GameDB.GameSettings[idx].blending           = myConfig.blending;
        GameDB.GameSettings[idx].key_click_disable  = myConfig.key_click_disable;
        GameDB.GameSettings[idx].keyboard_type      = myConfig.keyboard_type;
        GameDB.GameSettings[idx].dpad_type          = myConfig.dpad_type;
        GameDB.GameSettings[idx].disk_speedup       = myConfig.disk_speedup;
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
// Snap out the XEGS.DAT to the SD card. This is only done when the user asks for it 
// to be written out... either by holding both L/R shoulder buttons on the DS for a
// full half-second or by pressing START while in the configuration area. 
// -------------------------------------------------------------------------------------
void WriteGlobalSettings(void)
{
    FILE *fp;

    GameDB.db_version = GAME_DATABASE_VERSION;

    GameDB.default_tv_type            = myConfig.tv_type;
    GameDB.default_ram_type           = myConfig.ram_type;
    GameDB.default_palette_type       = myConfig.palette_type;
    GameDB.default_os_type            = myConfig.os_type;
    GameDB.default_basic_type         = myConfig.basic_type;
    GameDB.default_skip_frames        = myConfig.skip_frames;
    GameDB.default_keyboard_type      = myConfig.keyboard_type;
    GameDB.default_key_click_disable  = myConfig.key_click_disable;
    GameDB.default_auto_fire          = myConfig.auto_fire;
    GameDB.default_blending           = myConfig.blending;
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

    // -------------------------------------------------------------------------
    // We want to rename the older XEGS-DS.DAT to the new rebranded A8DS.DAT
    // -------------------------------------------------------------------------
    fp = fopen("/data/XEGS-DS.DAT", "rb");
    if (fp != NULL)
    {
        fclose(fp);
        rename("/data/XEGS-DS.DAT", "/data/A8DS.DAT");
    }
    
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
        if ((GameDB.db_version != GAME_DATABASE_VERSION) || (GameDB.checksum != checksum))
        {
            InitGameSettings();
        }
    }
    else
    {
        InitGameSettings();
    }

    myConfig.tv_type            = GameDB.default_tv_type;
    myConfig.palette_type       = GameDB.default_palette_type;
    myConfig.os_type            = GameDB.default_os_type;
    myConfig.basic_type         = GameDB.default_basic_type;
    myConfig.auto_fire          = GameDB.default_auto_fire;
    myConfig.ram_type           = GameDB.default_ram_type;
    myConfig.skip_frames        = GameDB.default_skip_frames;
    myConfig.keyboard_type      = GameDB.default_keyboard_type;
    myConfig.key_click_disable  = GameDB.default_key_click_disable;
    myConfig.blending           = GameDB.default_blending;
    myConfig.ram_type           = GameDB.default_ram_type;
    
    for (int i=0; i<8; i++) myConfig.keyMap[i] = GameDB.default_keyMap[i];
    if (myConfig.fps_setting == 2) full_speed=1; else full_speed=0;

    // ----------------------------------------------------------------------
    // Map the  ram_type to actual ram_size for use by the emulator...
    // ----------------------------------------------------------------------
    if      (myConfig.ram_type == 0) ram_size = RAM_128K;
    else if (myConfig.ram_type == 1) ram_size = RAM_320_RAMBO;
    else if (myConfig.ram_type == 2) ram_size = RAM_1088K;
    else                             ram_size = RAM_48K;

    // ---------------------------------------------------------------------------------------------
    // Sanity check... make sure if the user chose some odd combo of RAM and OS we fix it up...
    // ---------------------------------------------------------------------------------------------
    if (ram_size == RAM_48K) // If 48K... make sure we have OS set to one of the XLs
    {
        if ((myConfig.os_type == OS_ALTIRRA_XL) || (myConfig.os_type == OS_ATARI_XL))
        {
            myConfig.os_type = OS_ALTIRRA_800;
        }
    }
    else    // Must be 128K or 320K so make sure we aren't using older Atari 800 BIOS
    {
        if ((myConfig.os_type == OS_ALTIRRA_800) || (myConfig.os_type == OS_ATARI_OSB))
        {
            myConfig.os_type = OS_ALTIRRA_XL;
        }
    }    
}

void SetMyConfigDefaults(void)
{
    myConfig.xOffset            = 32;
    myConfig.yOffset            = 24;
    myConfig.xScale             = 256;
    myConfig.yScale             = 256;
    myConfig.artifacting        = 0;
    myConfig.emulatorText       = true;
    myConfig.blending           = GameDB.default_blending;
    myConfig.skip_frames        = GameDB.default_skip_frames;
    myConfig.os_type            = GameDB.default_os_type;
    myConfig.ram_type           = GameDB.default_ram_type;
    myConfig.basic_type         = GameDB.default_basic_type;
    myConfig.tv_type            = GameDB.default_tv_type;
    myConfig.auto_fire          = GameDB.default_auto_fire;
    myConfig.key_click_disable  = GameDB.default_key_click_disable;
    myConfig.disk_speedup = 1;
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
        if (memcmp(GameDB.GameSettings[idx].game_hash, last_hash, 32) == 0) break;
    }

    full_speed = 0;
    if (idx < MAX_GAME_SETTINGS)    // We found a match in the database... use it!
    {
        myConfig.xOffset            = GameDB.GameSettings[idx].xOffset;
        myConfig.yOffset            = GameDB.GameSettings[idx].yOffset;
        myConfig.xScale             = GameDB.GameSettings[idx].xScale;
        myConfig.yScale             = GameDB.GameSettings[idx].yScale;
        myConfig.artifacting        = GameDB.GameSettings[idx].artifacting;
        myConfig.palette_type       = GameDB.GameSettings[idx].palette_type;
        myConfig.os_type            = GameDB.GameSettings[idx].os_type;
        myConfig.basic_type         = GameDB.GameSettings[idx].basic_type;
        myConfig.tv_type            = GameDB.GameSettings[idx].tv_type;
        myConfig.auto_fire          = GameDB.GameSettings[idx].auto_fire;
        myConfig.skip_frames        = GameDB.GameSettings[idx].skip_frames;
        myConfig.ram_type           = GameDB.GameSettings[idx].ram_type;
        myConfig.blending           = GameDB.GameSettings[idx].blending;
        myConfig.key_click_disable  = GameDB.GameSettings[idx].key_click_disable;
        myConfig.keyboard_type      = GameDB.GameSettings[idx].keyboard_type;
        myConfig.dpad_type          = GameDB.GameSettings[idx].dpad_type;
        myConfig.disk_speedup       = GameDB.GameSettings[idx].disk_speedup;
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
    if (force_basic_type != 99)
    {
        myConfig.basic_type = force_basic_type;
        if (!bFirstLoad) force_basic_type = 99;
    }    
    
    // ----------------------------------------------------------------------
    // Map the saved ram_type to actual ram_size for use by the emulator...
    // ----------------------------------------------------------------------
    if      (myConfig.ram_type == 0) ram_size = RAM_128K;
    else if (myConfig.ram_type == 1) ram_size = RAM_320_RAMBO;
    else if (myConfig.ram_type == 2) ram_size = RAM_1088K;
    else                             ram_size = RAM_48K;

    install_os();
}


// ---------------------------------------------------------------------------
// Write out the XEGS.DAT configuration file to capture the settings for
// each game.
// ---------------------------------------------------------------------------
void dsWriteConfig(void)
{
    dsPrintValue(3,0,0, (char*)"CONFIG");
    WriteGameSettings();

    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    dsPrintValue(3,0,0, (char*)"      ");
}

// ---------------------------------------------------------------------------
// Write out the XEGS.DAT configuration file to capture the settings for
// each game.
// ---------------------------------------------------------------------------
void dsWriteGlobalConfig(void)
{
    dsPrintValue(3,0,0, (char*)"GLOBAL");
    WriteGlobalSettings();

    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    dsPrintValue(3,0,0, (char*)"      ");
}

// -----------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off 
// their option choices for the currently running game into the XEGS.DAT
// configuration database. When games are loaded back up, XEGS.DAT is read
// to see if we have a match and the user settings can be restored for the 
// game.
// -----------------------------------------------------------------------------
struct options_t
{
    char *label;
    char *option[70];
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

#define KEY_MAP_TEXT {"JOY FIRE", "JOY UP", "JOY DOWN", "JOY LEFT", "JOY RIGHT", "CONSOLE START", "CONSOLE SEL", "CONSOLE OPT", "KEY SPACE", "KEY RETURN", "KEY ESC", "KEY BREAK",  \
                      "KEY A", "KEY B", "KEY C", "KEY D", "KEY E", "KEY F", "KEY G", "KEY H", "KEY I", "KEY J", "KEY K", "KEY L", "KEY M", "KEY N", "KEY O",                        \
                      "KEY P", "KEY Q", "KEY R", "KEY S", "KEY T", "KEY U", "KEY V", "KEY W", "KEY X", "KEY Y", "KEY Z", "KEY 0", "KEY 1", "KEY 2", "KEY 3",                        \
                      "KEY 4", "KEY 5", "KEY 6", "KEY 7", "KEY 8", "KEY 9", "KEY UP", "KEY DOWN", "KEY LEFT", "KEY RIGHT", "KEY SPARE1", "KEY SPARE2",                              \
                      "KEY SPARE3", "VERTICAL+", "VERTICAL++", "VERTICAL-", "VERTICAL--", "HORIZONTAL+", "HORIZONTAL++", "HORIZONTAL-", "HORIZONTAL--", "OFFSET DPAD", "SCALE DPAD"}

const struct options_t Option_Table[2][20] =
{
    // Page 1
    {
        {"TV TYPE",     {"NTSC",        "PAL"},                             &myConfig.tv_type,              OPT_NORMAL, 2,   "NTSC=60 FPS       ",   "WITH 262 SCANLINES",  "PAL=50 FPS        ",  "WITH 312 SCANLINES"},
        {"MACHINE TYPE",{"128K XL/XE",  "320K XL/XE", 
                         "1088K XL/XE", "48K ATARI800"},                    &myConfig.ram_type,             OPT_NORMAL, 4,   "128K STANDARD FOR ",   "MOST GAMES. 320K /",  "1088 FOR BIG GAMES",  "48K COMPATIBILITY "},
        {"OS TYPE",     {"ALTIRRA XL",  "ATARIXL.ROM",
                         "ALTIRRA 800", "ATARIOSB.ROM"},                    &myConfig.os_type,              OPT_NORMAL, 4,   "BUILT-IN ALTIRRA  ",   "USUALLY. FEW GAMES",  "REQUIRE ATARIXL OR",  "ATARIOSB TO WORK  "},
        {"BASIC",       {"DISABLED",    "ALTIRRA",      "ATARIBAS.ROM"},    &myConfig.basic_type,           OPT_NORMAL, 3,   "NORMALLY DISABLED ",   "EXCEPT FOR BASIC  ",  "GAMES THAT REQUIRE",  "THE CART INSERTED "},
        {"SKIP FRAMES", {"NO",          "MODERATE",     "AGGRESSIVE"},      &myConfig.skip_frames,          OPT_NORMAL, 3,   "OFF NORMALLY AS   ",   "SOME GAMES CAN    ",  "GLITCH WHEN SET   ",  "TO FRAMESKIP      "},
        {"PALETTE",     {"BRIGHT",      "MUTED"},                           &myConfig.palette_type,         OPT_NORMAL, 2,   "CHOOSE PALETTE    ",   "THAT BEST SUITS   ",  "YOUR VIEWING      ",  "PREFERENCE        "},
        
        {"FPS SETTING", {"OFF",         "ON", "ON-TURBO"},                  &myConfig.fps_setting,          OPT_NORMAL, 3,   "SHOW FPS ON MAIN  ",   "DISPLAY. OPTIONALY",  "RUN IN TURBO MODE ",  "FAST AS POSSIBLE  "},
        {"ARTIFACTING", {"OFF",         "1:BROWN/BLUE", "2:BLUE/BROWN",
                                        "3:RED/GREEN","4:GREEN/RED"},       &myConfig.artifacting,          OPT_NORMAL, 5,   "A FEW HIRES GAMES ",   "NEED ARTIFACING   ",  "TO LOOK RIGHT     ",  "OTHERWISE SET OFF "},
        {"BLENDING",    {"NORMAL",      "BLUR1", "BLUR2", "BLUR3", 
                         "BLUR4","BLUR5","BLUR6","BLUR7"},                  &myConfig.blending,             OPT_NORMAL, 8,   "NORMALLY BLUR6    ",   "AND VARIOUS BLUR  ",  "LEVELS WILL HELP  ",  "SCREEN SCALING.   "},
        {"DISK SPEEDUP",{"OFF",         "ON"},                              &myConfig.disk_speedup,         OPT_NORMAL, 2,   "NORMALLY ON IS    ",   "DESIRED TO SPEED  ",  "UP FLOPPY DISK    ",  "ACCESS. OFF=SLOW  "},
        {"KEY CLICK",   {"ON",          "OFF"},                             &myConfig.key_click_disable,    OPT_NORMAL, 2,   "NORMALLY ON       ",   "CAN BE USED TO    ",  "SILENCE KEY CLICKS",  "FOR KEYBOARD USE  "},
        {"EMULATOR TXT",{"OFF",         "ON"},                              &myConfig.emulatorText,         OPT_NORMAL, 2,   "NORMALLY ON       ",   "CAN BE USED TO    ",  "DISABLE FILENAME  ",  "INFO ON MAIN SCRN "},
        {"KEYBOARD",    {"800XL STYLE1","800XL STYLE2", 
                         "400 STYLE",  "130XE STYLE"},                      &myConfig.keyboard_type,        OPT_NORMAL, 4,   "CHOOSE THE STYLE  ",   "THAT BEST SUITS   ",  "YOUR TASTES.      ",  "                  "},
        {NULL,          {"",            ""},                                NULL,                           OPT_NORMAL, 2,   "HELP1             ",   "HELP2             ",  "HELP3             ",  "HELP4             "}
    },
    // Page 2
    {
        {"A BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[0],            OPT_KEYSEL, 65,  "SET THE A KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"B BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[1],            OPT_KEYSEL, 65,  "SET THE B KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"X BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[2],            OPT_KEYSEL, 65,  "SET THE X KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"Y BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[3],            OPT_KEYSEL, 65,  "SET THE Y KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"L BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[4],            OPT_KEYSEL, 65,  "SET THE L KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"R BUTTON",    KEY_MAP_TEXT,                                       &myConfig.keyMap[5],            OPT_KEYSEL, 65,  "SET THE R KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"START BTN",   KEY_MAP_TEXT,                                       &myConfig.keyMap[6],            OPT_KEYSEL, 65,  "SET START KEY TO  ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"SELECT BTN",  KEY_MAP_TEXT,                                       &myConfig.keyMap[7],            OPT_KEYSEL, 65,  "SET SELECT KEY TO ",   "DESIRED FUNCTION  ",  "JOYSTICK, KEYBOARD ", "OR META BUTTON.   "},
        {"D-PAD",       {"JOY 1", "JOY 2", "DIAGONALS", "CURSORS"},         &myConfig.dpad_type,            OPT_NORMAL, 4,   "CHOOSE HOW THE    ",   "JOYSTICK OPERATES ",  "CAN SWAP JOY1 AND ",  "JOY2 OR MAP CURSOR"},    
        {"AUTOFIRE",    {"OFF",         "SLOW",   "MED",  "FAST"},          &myConfig.auto_fire,            OPT_NORMAL, 4,   "TOGGLE AUTOFIRE   ",   "SLOW = 4x/SEC     ",  "MED  = 8x/SEC     ",  "FAST = 15x/SEC    "},
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
            dsPrintValue(1,5+len, (len==0 ? 1:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<16; i++) 
        {
            dsPrintValue(1,5+i, 0, (char *)"                               ");
        }
    }

    dsPrintValue(0,22, 0, (char *)"B=EXIT X=MORE START=SAVE SEL=DEF");
    return len;    
}


// -----------------------------------------------------------------------------
// Allows the user to move the cursor up and down through the various table 
// enties  above to select options for the game they wish to play. 
// -----------------------------------------------------------------------------
void dsChooseOptions(int bOkayToChangePalette)
{
    static int last_pal=-1;
    static int last_art=-1;
    static char strBuf[64];
    short int optionHighlighted;
    short int idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;
    int key_repeat_counter = 0;

    // Show the background...
    decompress(bgFileSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(bgFileSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void *) bgFileSelPal,(u16*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b) +31*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

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
                    sprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    sprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,5+optionHighlighted,1, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    sprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                if (Option_Table[option_table][optionHighlighted].option_type == OPT_NUMERIC)
                {
                    siprintf(strBuf, " %-12s : %-13d ", Option_Table[option_table][optionHighlighted].label, *((short int*)Option_Table[option_table][optionHighlighted].option_val));
                }
                else
                {
                    sprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,5+optionHighlighted,1, strBuf);
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
                    sprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,5+optionHighlighted,1, strBuf);
                if (strcmp(Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)], "ATARIXL.ROM")==0)
                {
                    if (!bAtariOS) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else if (strcmp(Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)], "ATARIOSB.ROM")==0)
                {
                    if (!bAtariOSB) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else if (strcmp(Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)], "ATARIBAS.ROM")==0)
                {
                    if (!bAtariBASIC) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else dsPrintValue(0,0,0,"           ");
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
                    sprintf(strBuf, " %-12s : %-13s ", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                }
                dsPrintValue(1,5+optionHighlighted,1, strBuf);
                if (strcmp(Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)], "ATARIXL.ROM")==0)
                {
                    if (!bAtariOS) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else if (strcmp(Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)], "ATARIOSB.ROM")==0)
                {
                    if (!bAtariOSB) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else if (strcmp(Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)], "ATARIBAS.ROM")==0)
                {
                    if (!bAtariBASIC) dsPrintValue(0,0,0,"ROM MISSING");
                }
                else dsPrintValue(0,0,0,"           ");
            }            
            if (keysCurrent() & KEY_START)  // Save Options
            {
                if (bOkayToChangePalette)   // This lets us know that a game is selected...
                {
                    dsWriteConfig();
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
                break;
            }

            // In case the Artifacting global changed....
            if (last_art != myConfig.artifacting)
            {
                last_art = myConfig.artifacting;
                ANTIC_UpdateArtifacting();
            }
            
            // In case the Pllette global changed....
            if (last_pal != myConfig.palette_type)
            {
                last_pal = myConfig.palette_type;
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

    if (myConfig.fps_setting == 2) full_speed=1; else full_speed=0;

    // ----------------------------------------------------------------------
    // Map the  ram_type to actual ram_size for use by the emulator...
    // ----------------------------------------------------------------------
    if      (myConfig.ram_type == 0) ram_size = RAM_128K;
    else if (myConfig.ram_type == 1) ram_size = RAM_320_RAMBO;
    else if (myConfig.ram_type == 2) ram_size = RAM_1088K;
    else                             ram_size = RAM_48K;

    // ---------------------------------------------------------------------------------------------
    // Sanity check... make sure if the user chose some odd combo of RAM and OS we fix it up...
    // ---------------------------------------------------------------------------------------------
    if (ram_size == RAM_48K) // If 48K... make sure we have OS set to one of the XLs
    {
        if ((myConfig.os_type == OS_ALTIRRA_XL) || (myConfig.os_type == OS_ATARI_XL))
        {
            myConfig.os_type = OS_ALTIRRA_800;
        }
    }
    else    // Must be 128K or 320K so make sure we aren't using older Atari 800 BIOS
    {
        if ((myConfig.os_type == OS_ALTIRRA_800) || (myConfig.os_type == OS_ATARI_OSB))
        {
            myConfig.os_type = OS_ALTIRRA_XL;
        }
    }

    install_os();

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


// End of file

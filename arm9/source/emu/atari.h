#ifndef _ATARI_H_
#define _ATARI_H_

#include "config.h"
#include <stdio.h> /* FILENAME_MAX */
#ifdef WIN32
#include <windows.h>
#endif

extern int debug[];

#define OS_ALTIRRA      0
#define OS_ATARI        1

#define BASIC_ALTIRRA   0
#define BASIC_ATARIREVC 1

extern int os_type;

#define DISK_XEX    0
#define DISK_1      1
#define DISK_2      2

#define DISK_MAX    3

extern char disk_filename[DISK_MAX][256];


/* Fundamental declarations ---------------------------------------------- */

//#define DIRTYRECT
#define NODIRTYCOMPARE

#define PAGED_ATTRIB 1

#define ATARI_TITLE  "XEGS-DS"

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

#define TV_PAL 312
#define TV_NTSC 262

// Some global sound defines
#define SOUND_FREQ  (tv_mode == TV_NTSC ? 15720:15600)      // 60 frames per second. 264 scanlines per frame. 1 samples per scanline. 60*264*1 = 15720... slightly different for pal 50*312*1=15600
#define SNDLENGTH  4096                                     // Must be power of 2... so we can quicly mask it

/* SBYTE and UBYTE must be exactly 1 byte long. */
/* SWORD and UWORD must be exactly 2 bytes long. */
/* SLONG and ULONG must be exactly 4 bytes long. */
#define SBYTE signed char
#define SWORD signed short
#define SLONG signed int
#define UBYTE unsigned char
#define UWORD unsigned short
#ifndef WIN32
/* Windows headers typedef ULONG */
#define ULONG unsigned int
#endif
/* Note: in various parts of the emulator we assume that char is 1 byte
   and int is 4 bytes. */


/* Public interface ------------------------------------------------------ */

/* Machine type. */
#define MACHINE_OSA   0
#define MACHINE_OSB   1
#define MACHINE_XLXE  2
#define MACHINE_5200  3

#define Atari800_MACHINE_OSA   MACHINE_OSA
#define Atari800_MACHINE_OSB   MACHINE_OSB
#define Atari800_MACHINE_XLXE  MACHINE_XLXE
#define Atari800_MACHINE_5200  MACHINE_5200

extern int machine_type;
#define Atari800_machine_type machine_type

/* RAM size in kilobytes.
   Valid values for MACHINE_OSA and MACHINE_OSB are: 16, 48, 52.
   Valid values for MACHINE_XLXE are: 128, RAM_320_RAMBO */
#define RAM_128K            128
#define RAM_320_RAMBO       320
extern int ram_size;

/* Always call Atari800_InitialiseMachine() after changing machine_type
   or ram_size! */

/* Video system. */
#define TV_NTSC         262
#define TV_PAL          312
extern int tv_mode;

/* TRUE to disable Atari BASIC when booting Atari (hold Option in XL/XE). */
extern int disable_basic;

/* Dimensions of atari_screen.
   atari_screen is ATARI_WIDTH * ATARI_HEIGHT bytes.
   Each byte is an Atari color code - use Palette_Get[RGB] functions
   to get actual RGB codes.
   You should never display anything outside the middle 336 columns. */
#define ATARI_WIDTH  384
#define ATARI_HEIGHT 240

/* Special key codes.
   Store in key_code. */
#define AKEY_WARMSTART             -2
#define AKEY_COLDSTART             -3
#define AKEY_EXIT                  -4
#define AKEY_BREAK                 -5
#define AKEY_UI                    -7
#define AKEY_SCREENSHOT            -8
#define AKEY_SCREENSHOT_INTERLACE  -9
#define AKEY_START                 -10
#define AKEY_SELECT                -11
#define AKEY_OPTION                -12

/* Menu codes for Alt+letter shortcuts.
   Store in alt_function and put AKEY_UI in key_code. */
#define MENU_DISK             0
#define MENU_CARTRIDGE        1
#define MENU_RUN              2
#define MENU_SYSTEM           3
#define MENU_SOUND            4
#define MENU_SOUND_RECORDING  5
#define MENU_DISPLAY          6
#define MENU_SETTINGS         7
#define MENU_SAVESTATE        8
#define MENU_LOADSTATE        9
#define MENU_PCX              10
#define MENU_PCXI             11
#define MENU_BACK             12
#define MENU_RESETW           13
#define MENU_RESETC           14
#define MENU_MONITOR          15
#define MENU_ABOUT            16
#define MENU_EXIT             17
#define MENU_CASSETTE         18

/* File types returned by AFILE_DetectFileType() and AFILE_OpenFile(). */
#define AFILE_ERROR      0
#define AFILE_ATR        1
#define AFILE_XFD        2
#define AFILE_ATR_GZ     3
#define AFILE_XFD_GZ     4
#define AFILE_DCM        5
#define AFILE_XEX        6
#define AFILE_BAS        7
#define AFILE_LST        8
#define AFILE_CART       9
#define AFILE_ROM        10
#define AFILE_CAS        11
#define AFILE_BOOT_TAPE  12
#define AFILE_STATE      13
#define AFILE_STATE_GZ   14
#define AFILE_PRO        15
#define AFILE_ATX        16

#define ATR_Header AFILE_ATR_Header
/* ATR format header */
struct AFILE_ATR_Header {
	unsigned char magic1;
	unsigned char magic2;
	unsigned char seccountlo;
	unsigned char seccounthi;
	unsigned char secsizelo;
	unsigned char secsizehi;
	unsigned char hiseccountlo;
	unsigned char hiseccounthi;
	unsigned char gash[7];
	unsigned char writeprotect;
};

/* First two bytes of an ATR file. */
#define AFILE_ATR_MAGIC1  0x96
#define AFILE_ATR_MAGIC2  0x02
#define MAGIC1  0x96
#define MAGIC2  0x02

extern int gTotalAtariFrames;

/* Initializes Atari800 emulation core. */
int Atari800_Initialise(void);

/* Emulates one frame (1/50sec for PAL, 1/60sec for NTSC). */
void Atari800_Frame(void);

#define Atari800_Coldstart Coldstart
#define Atari800_Warmstart Warmstart

/* Reboots the emulated Atari. */
void Coldstart(void);

/* Presses the Reset key in the emulated Atari. */
void Warmstart(void);

/* Reinitializes after machine_type or ram_size change.
   You should call Coldstart() after it. */
int Atari800_InitialiseMachine(void);

/* Reinitializes patches after enable_*_patch change. */
void Atari800_UpdatePatches(void);

/* Auto-detects file type and returns one of AFILE_* values. */
int Atari800_DetectFileType(const char *filename);

/* Auto-detects file type and mounts the file in the emulator.
   reboot: Coldstart() for disks, cartridges and tapes
   diskno: drive number for disks (1-8)
   readonly: mount disks as read-only */
int Atari800_OpenFile(const char *filename, int reboot, int diskno, int readonly, int bEnableBasic);

/* Load Atari800 text configuration file. */
int Atari800_LoadConfig(const char *alternate_config_filename);

/* Writes Atari800 text configuration file. */
int Atari800_WriteConfig(void);

/* Shuts down Atari800 emulation core. */
int Atari800_Exit(int run_monitor);


/* Current clock cycle in a scanline.
   Normally 0 <= xpos && xpos < LINE_C, but in some cases xpos >= LINE_C,
   which means that we are already in line (ypos + 1). */
extern int xpos;

/* xpos limit for the currently running 6502 emulation. */
extern int xpos_limit;

/* Number of cycles per scanline. */
#define LINE_C   114

/* STA WSYNC resumes here. */
#define WSYNC_C  106

/* Number of memory refresh cycles per scanline.
   In the first scanline of a font mode there are actually less than DMAR
   memory refresh cycles. */
#define DMAR     9

/* Number of scanlines per frame. */
#define max_ypos tv_mode

/* Main clock value at the beginning of the current scanline. */
extern unsigned int screenline_cpu_clock;

/* Current main clock value. */
#define cpu_clock (screenline_cpu_clock + xpos)

#define UNALIGNED_STAT_DEF(stat_arr)
#define UNALIGNED_STAT_DECL(stat_arr)
#define UNALIGNED_GET_WORD(ptr, stat_arr)        (*(const UWORD *) (ptr))
#define UNALIGNED_PUT_WORD(ptr, value, stat_arr) (*(UWORD *) (ptr) = (value))
#define UNALIGNED_GET_LONG(ptr, stat_arr)        (*(const ULONG *) (ptr))
#define UNALIGNED_PUT_LONG(ptr, value, stat_arr) (*(ULONG *) (ptr) = (value))

/* Reads a byte from the specified special address (not RAM or ROM). */
UBYTE Atari800_GetByte(UWORD addr);

/* Stores a byte at the specified special address (not RAM or ROM). */
void Atari800_PutByte(UWORD addr, UBYTE byte);

/* Installs SIO patch and disables ROM checksum test. */
void Atari800_PatchOS(void);

#endif /* _ATARI_H_ */

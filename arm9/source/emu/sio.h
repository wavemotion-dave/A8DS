/*
 * SIO.C contains the emulation of the Serial I/O on the Atari 800.
 *
 * The baseline for this file is the Atari800 2.0.x source and has
 * been heavily modified for optimization on the Nintendo DS/DSi.
 * Atari800 has undergone numerous improvements and enhancements
 * since the time this file was used as a baseline for A8DS and 
 * it is strongly recommended you seek out the latest Atari800 sources.
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
 * be distributed using that same licensing model. See COPYING for the full license
 * but the original Atari800 copyright notice retained below:
 */

/*
 * sio.c - Serial I/O emulation
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2010 Atari800 development team (see DOC/CREDITS)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef SIO_H_
#define SIO_H_

#include "atari.h"
#include "a8ds.h"

#define SIO_MAX_DRIVES 4

/* Serial I/O emulation support */
#define SIO_NoFrame         (0x00)
#define SIO_CommandFrame    (0x01)
#define SIO_StatusRead      (0x02)
#define SIO_ReadFrame       (0x03)
#define SIO_WriteFrame      (0x04)
#define SIO_FinalStatus     (0x05)
#define SIO_FormatFrame     (0x06)
#define SIO_CasRead         (0x60)
#define SIO_CasWrite        (0x61)

typedef enum SIO_tagUnitStatus {
    SIO_OFF,
    SIO_NO_DISK,
    SIO_READ_ONLY,
    SIO_READ_WRITE
} SIO_UnitStatus;

extern SIO_UnitStatus SIO_drive_status[SIO_MAX_DRIVES];
extern char SIO_filename[SIO_MAX_DRIVES][MAX_FILENAME];
extern int SIO_last_drive;
extern UBYTE CommandFrame[6];
extern int CommandIndex;
extern UBYTE DataBuffer[256 + 3];
extern int DataIndex;
extern int TransferStatus;
extern int ExpectedBytes;


#define SIO_LAST_READ 0
#define SIO_LAST_WRITE 1
extern int SIO_last_op;
extern int SIO_last_op_time;
extern int SIO_last_drive; /* 1 .. 4 */
extern int SIO_last_sector;

int SIO_Mount(int diskno, const char *filename, int b_open_readonly);
void SIO_Dismount(int diskno);
void SIO_DisableDrive(int diskno);
int SIO_RotateDisks(void);
void SIO_Handler(void);

UBYTE SIO_ChkSum(const UBYTE *buffer, int length);
void SIO_TapeMotor(int onoff);
void SIO_SwitchCommandFrame(int onoff);
void SIO_PutByte(int byte);
int SIO_GetByte(void);
int SIO_Initialise(int *argc, char *argv[]);
void SIO_Exit(void);

/* Some defines about the serial I/O timing. Currently fixed! */
#define SIO_XMTDONE_INTERVAL  15
#define SIO_SERIN_INTERVAL     8
#define SIO_SEROUT_INTERVAL    8
#define SIO_ACK_INTERVAL      36

/* These functions are also used by the 1450XLD Parallel disk device */
int SIO_ReadStatusBlock(int unit, UBYTE *buffer);
int SIO_FormatDisk(int unit, UBYTE *buffer, int sectsize, int sectcount);
void SIO_SizeOfSector(UBYTE unit, int sector, int *sz, ULONG *ofs);
int SIO_ReadSector(int unit, int sector, UBYTE *buffer);
int SIO_DriveStatus(int unit, UBYTE *buffer);
int SIO_WriteStatusBlock(int unit, const UBYTE *buffer);
int SIO_WriteSector(int unit, int sector, const UBYTE *buffer);
void SIO_StateSave(void);
void SIO_StateRead(void);

#endif  /* SIO_H_ */

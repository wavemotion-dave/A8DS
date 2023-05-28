#ifndef SIO_H_
#define SIO_H_

#include <stdio.h> /* FILENAME_MAX */

#include "atari.h"

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
extern char SIO_filename[SIO_MAX_DRIVES][FILENAME_MAX];
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

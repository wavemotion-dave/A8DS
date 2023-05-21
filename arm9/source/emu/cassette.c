// Obsoleted - not used for A8DS

#include <stdio.h>
#include <string.h>

#include "atari.h"
#include "cpu.h"
#include "cassette.h"
#include "memory.h"
#include "sio.h"
#include "util.h"
#include "pokey.h"

int CASSETTE_hold_start_on_reboot = 0;
int CASSETTE_hold_start = 0;
int CASSETTE_press_space = 0;

int CASSETTE_Initialise(int *argc, char *argv[])
{
    return TRUE;
}

int CASSETTE_CheckFile(const char *filename, FILE **fp, char *description, int *last_block, int *isCAS)
{
    return TRUE;
}

int CASSETTE_CreateFile(const char *filename, FILE **fp, int *isCAS)
{
    return TRUE;
}

int CASSETTE_Insert(const char *filename)
{
    return TRUE;
}

void CASSETTE_Remove(void)
{
}

int CASSETTE_AddGap(int gaptime)
{
    return 1;
}

void CASSETTE_LeaderLoad(void)
{
}

void CASSETTE_LeaderSave(void)
{
}

int CASSETTE_Read(void)
{
    return 0;
}

int CASSETTE_Write(int length)
{
    return 0;
}

int CASSETTE_GetByte(void)
{
    return 0;
}

int CASSETTE_IOLineStatus(void)
{
    return 0;
}

int CASSETTE_GetInputIRQDelay(void)
{
    return 0;
}

int CASSETTE_IsSaveFile(void)
{
    return 0;
}

void CASSETTE_PutByte(int byte)
{
}

void CASSETTE_TapeMotor(int onoff)
{
}

void CASSETTE_AddScanLine(void)
{
}


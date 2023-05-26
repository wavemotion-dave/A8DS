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

void CASSETTE_LeaderLoad(void)
{
}

void CASSETTE_LeaderSave(void)
{
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


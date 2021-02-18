// ----------------------------------------------------------------------------
// Hash.h
// ----------------------------------------------------------------------------
#ifndef HASH_H
#define HASH_H

#include <string.h>

typedef unsigned int uint;
typedef unsigned char byte;

extern void hash_Compute(const byte* source, uint length, byte * dest);

#endif

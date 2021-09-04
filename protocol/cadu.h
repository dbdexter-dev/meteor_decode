#ifndef cadu_h
#define cadu_h

#include <stdint.h>
#include "vcdu.h"

#define CADU_DATA_LENGTH 1020
#define CONV_CADU_LEN (2*sizeof(Cadu))
#define CADU_SOFT_LEN (2*8*sizeof(Cadu))

#define SYNCWORD 0x1ACFFC1D
#define SYNC_LEN 4

/* Data link layer, with added sync markers and pseudorandom noise */
typedef struct {
	uint32_t syncword;
	Vcdu data;
} __attribute__((packed)) Cadu;

#endif /* cadu_h */

#ifndef LRPTDEC_PACKETPROC_H
#define LRPTDEC_PACKETPROC_H

#include <stdlib.h>
#include <stdint.h>
#include "packet.h"
#include "reedsolomon.h"
#include "source.h"

#define NOISE_PERIOD 255
#define MAX_PKT_SIZE (14 * MCU_SIZE)

typedef struct {
	int seq;
	int apid;
	int len;
	int has_sec_hdr;

	uint8_t data[MAX_PKT_SIZE];
} Segment;

typedef struct {
	void *next_header;
	uint8_t cadu[sizeof(Cadu)];
	ReedSolomon *rs;
	HardSource *src;
} PktProcessor;

PktProcessor* pkt_init(HardSource *src);
void          pkt_deinit(PktProcessor *pp);

int pkt_get_next(PktProcessor *pp, Segment *seg);

#endif

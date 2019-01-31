#ifndef LRPTDEC_PACKETIZER_H
#define LRPTDEC_PACKETIZER_H

#include <stdlib.h>
#include <stdint.h>
#include "packet.h"
#include "reedsolomon.h"
#include "source.h"

#define NOISE_PERIOD 255
#define MAX_PKT_SIZE 1024

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

int pkt_read(PktProcessor *pp, Segment *seg);

#endif

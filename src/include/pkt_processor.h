#ifndef LRPTDEC_PACKETPROC_H
#define LRPTDEC_PACKETPROC_H

#include <stdint.h>
#include "packet.h"
#include "reedsolomon.h"

#define NOISE_PERIOD 255
#define MAX_PKT_SIZE (196 * MCU_SIZE)

typedef struct {
	int offset;
	ReedSolomon *rs;
} PktProcessor;

PktProcessor* pkt_init(void);
void          pkt_deinit(PktProcessor *pp);

int pkt_process(PktProcessor *self, Cvcdu *p);

#endif

#ifndef LRPTDEC_PACKET_H
#define LRPTDEC_PACKET_H

#include <stdint.h>

struct _cadu;

#define FRAME_SIZE (sizeof(struct _cadu))
#define FRAME_BITS (FRAME_SIZE << 3)
#define SOFT_FRAME_SIZE (FRAME_BITS * 2)
#define INTERLEAVING 4

typedef struct {
	/* Primary header */
	uint8_t info[2]; 
	uint8_t counter[3];
	uint8_t signalling;
	uint8_t coding_ctrl[2];

	/* Data unit zone */
	uint8_t mpdu_header[2];
	uint8_t mpdu_data[882];

	uint8_t reed_solomon[128];
} Cvcdu;

typedef struct _cadu {
	uint32_t sync_marker;
	Cvcdu cvcdu;
} Cadu;

const uint8_t SYNCWORD[4];

void     packet_init();
void     packet_descramble(Cvcdu *p);
uint32_t packet_vcdu_counter(Cvcdu *p);
void     packet_vcdu_dump(Cvcdu *p);
uint8_t  packet_vcdu_spacecraft(const Cvcdu *p);

#endif


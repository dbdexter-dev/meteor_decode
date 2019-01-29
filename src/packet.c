#include <stdio.h>
#include <stdint.h>
#include "hexdump.h"
#include "packet.h"

#define NOISE_PERIOD 255

const uint8_t SYNCWORD[] = {0x1a, 0xcf, 0xfc, 0x1d};

static void compute_noise();
static uint8_t _noise[NOISE_PERIOD];
static int vcdu_header_present(Cvcdu *p);
static int vcdu_header_ptr(Cvcdu *p);


/* Byte by byte, xor the packet with the added noise */
void
packet_descramble(Cvcdu *p)
{
	int i;

	for (i=0; i<(int)sizeof(*p); i++) {
		((char*)p)[i] ^= _noise[i % NOISE_PERIOD];
	}
}

void
packet_init()
{
	compute_noise();
}

uint32_t
packet_vcdu_counter(Cvcdu *p)
{
	return (p->counter[0] << 16) | (p->counter[1] << 8) | (p->counter[2]);
}

uint8_t 
packet_vcdu_spacecraft(const Cvcdu *p)
{
	return (p->info[0] & 0x3F) << 2 | (p->info[1] >> 5);
}

void
packet_vcdu_dump(Cvcdu *p)
{
	int header_presence;

	header_presence = vcdu_header_present(p);

	printf("[%d] ", packet_vcdu_counter(p));
	printf("Spacecraft ID: 0x%02x\n", packet_vcdu_spacecraft(p));
	printf("MPDU Header: %s\n", header_presence ? "present" : "absent");
	if (header_presence) {
		printf("MPDU header offset: %d\n", vcdu_header_ptr(p));
	}
}


/* Static functions {{{*/
static int
vcdu_header_present(Cvcdu *p)
{
	return !(p->mpdu_header[0] >> 3);
}

static int
vcdu_header_ptr(Cvcdu *p)
{
	return (p->mpdu_header[0] & 0x07) << 8 | p->mpdu_header[1];
}

/* Generate the pseudorandom noise the packets are scrambled with */
/* Generator poly: x^8 + x^7 + x^5 + x^3 + 1 */
static void
compute_noise()
{
	uint8_t state, out, accum;
	int i, j;

	state = 0xFF;
	for (i=0; i<NOISE_PERIOD; i++) {
		accum = 0;
		for (j=0; j<8; j++) {
			out = ((state >> 7) ^ (state >> 5) ^ (state >> 3) ^ (state >> 0));
			accum = (accum << 1) | (state >> 0 & 0x01);
			state = (state >> 1) | (out << 7);
		}
		_noise[i] = accum;
	}
}

/*}}}*/

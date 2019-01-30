#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hexdump.h"
#include "packet.h"
#include "pkt_processor.h"
#include "reedsolomon.h"
#include "utils.h"
#include <time.h>


static void compute_noise(void);
static void descramble(Cvcdu *p);

static uint8_t _noise[NOISE_PERIOD];
static int     _initialized = 0;

PktProcessor*
pkt_init()
{
	PktProcessor *ret;

	if (!_initialized) {
		compute_noise();
	}

	ret = safealloc(sizeof(*ret));
	ret->rs = rs_init(sizeof(Cvcdu), INTERLEAVING);

	return ret;
}

void
pkt_deinit(PktProcessor *pp)
{
	rs_deinit(pp->rs);
	free(pp);
}

int
pkt_process(PktProcessor *self, Cvcdu *p)
{
	int rs_fix_count;
	time_t time;
	Mpdu *mpdu;
	int len, apid;

	/* Descramble */
	descramble(p);

	/* Reed-Solomon error correct, exit on failure */
	rs_fix_count = rs_fix_packet(self->rs, p, NULL);
	if (rs_fix_count < 0) {
		return -1;
	}

	/* Get the inner header */
	mpdu = vcdu_header_ptr(p);
	if (mpdu) {
		len = mpdu_len(mpdu) - sizeof(Timestamp) + 1;
		apid = mpdu_apid(mpdu);
		time = mpdu_msec(mpdu) + 946728000;

		printf("(%2d) Instrument %d, APID %d\tSegment size: %d\t Onboard time: %s", 
		        rs_fix_count, vcdu_vcid(p), apid, len, timeofday(time));

		switch(apid) {
		case 64:
		case 65:
		case 66:
		case 67:
		case 68:
		case 69:
			break;
		case 70:
			printf("\nOffset: %d\n", vcdu_header_offset(p));
			hexdump("Data", mpdu, 884);
			break;
		default:
			break;
		}

	} else {
		/* TODO wat do here? */
	}
	return rs_fix_count;

}

/* Static functions {{{*/
/* Byte by byte, xor the packet with the added noise */
static void
descramble(Cvcdu *p)
{
	int i;

	for (i=0; i<(int)sizeof(*p); i++) {
		((char*)p)[i] ^= _noise[i % NOISE_PERIOD];
	}
}

/* Generate the pseudorandom noise the packets are scrambled with, using a LFSR.
 * Generator poly: x^8 + x^7 + x^5 + x^3 + 1 */
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

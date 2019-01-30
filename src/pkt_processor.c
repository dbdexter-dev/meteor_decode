#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hexdump.h"
#include "packet.h"
#include "pkt_processor.h"
#include "reedsolomon.h"
#include "source.h"
#include "utils.h"

static void compute_noise(void);
static void descramble(Cvcdu *p);

static uint8_t _noise[NOISE_PERIOD];
static int     _initialized = 0;

PktProcessor*
pkt_init(HardSource *src)
{
	PktProcessor *ret;

	if (!_initialized) {
		compute_noise();
	}

	ret = safealloc(sizeof(*ret));
	ret->rs = rs_init(sizeof(Cvcdu), INTERLEAVING);
	ret->src = src;
	ret->next_header = NULL;

	return ret;
}

void
pkt_deinit(PktProcessor *pp)
{
	rs_deinit(pp->rs);
	pp->src->close(pp->src);
	free(pp);
}

/* Retrieve the next packet */
int
pkt_get_next(PktProcessor *self, Segment *seg)
{
	Cvcdu *vcdu;
	Mpdu *mpdu, frag_hdr;
	uint8_t *data_ptr;
	int rs_fix_count;
	int bytes_out, bytes_in;

	vcdu = &(((Cadu*)self->cadu)->cvcdu);

	bytes_out = 0;
	if (!self->next_header) {
		/* Start reading brand new frames */
		bytes_in = self->src->read(self->src, self->cadu, sizeof(self->cadu));
		/* Check whether we've reached the end of the input source */
		if (bytes_in < (int)sizeof(Cadu)) {
			seg->len = 0;
			return 0;
		}

		/* Descramble and error-correct the vcdu */
		descramble(vcdu);
		rs_fix_count = rs_fix_packet(self->rs, vcdu, NULL);

		/* Check RS return status */
		if (rs_fix_count < 0) {
			self->next_header = NULL;
			seg->len = 0;
			return -1;
		}

		mpdu = vcdu_mpdu_header_ptr(vcdu);
		self->next_header = mpdu;
	} else {
		/* Read from the local CADU */
		mpdu = (Mpdu*)(self->next_header);
	}

	/* Check whether the header is fragmented. If it is, reconstruct it locally
	 * before trying to parse the data */
	if ((uint8_t*)mpdu + MPDU_HDR_SIZE > (uint8_t*)vcdu->mpdu_data + MPDU_DATA_SIZE) {
		bytes_out = MPDU_DATA_SIZE - ((uint8_t*)mpdu - vcdu->mpdu_data);
		printf("Fragmented header, %d bytes left\n", bytes_out);
		memcpy(&frag_hdr, mpdu, bytes_out);

		bytes_in = self->src->read(self->src, self->cadu, sizeof(self->cadu));
		if (bytes_in < (int)sizeof(Cadu)) {
			seg->len = 0;
			return 0;
		}

		/* Descramble and error-correct the vcdu */
		descramble(vcdu);
		rs_fix_count = rs_fix_packet(self->rs, vcdu, NULL);

		/* Check RS return status */
		if (rs_fix_count < 0) {
			self->next_header = NULL;
			seg->len = 0;
			return -1;
		}

		memcpy((uint8_t*)&frag_hdr + bytes_out, vcdu->mpdu_data, MPDU_HDR_SIZE - bytes_out);
		self->next_header = vcdu_mpdu_header_ptr(vcdu);

		seg->len = mpdu_data_len(&frag_hdr);
		seg->apid = mpdu_apid(&frag_hdr);
		seg->seq = mpdu_seq(&frag_hdr);
		seg->has_sec_hdr = mpdu_has_sec_hdr(&frag_hdr);

		/* "Fake" the header as if it were before the beginning of the mpdu */
		data_ptr = (uint8_t*)vcdu->mpdu_data + (MPDU_HDR_SIZE - bytes_out);
		mpdu = (Mpdu*)((uint8_t*)vcdu->mpdu_data - (MPDU_HDR_SIZE - bytes_out));
	} else {
		seg->len = mpdu_data_len(mpdu);
		seg->apid = mpdu_apid(mpdu);
		seg->seq = mpdu_seq(mpdu);
		seg->has_sec_hdr = mpdu_has_sec_hdr(mpdu);

		data_ptr = mpdu_data_ptr(mpdu);
	}

	/* Check whether the data is fragmented across multiple VCDUs */
	if (data_ptr + seg->len > (uint8_t*)vcdu->mpdu_data + MPDU_DATA_SIZE) {
		/* Packet is fragmented: write what we have and grab a new VCDU */
		bytes_out = MPDU_DATA_SIZE - (data_ptr - vcdu->mpdu_data);
		printf("Fragmented data, %d bytes left\n", bytes_out);
		memcpy(seg->data, data_ptr, bytes_out);

		bytes_in = self->src->read(self->src, self->cadu, sizeof(self->cadu));
		if (bytes_in < (int)sizeof(Cadu)) {
			seg->len = 0;
			return 0;
		}

		/* Descramble and error-correct the vcdu */
		descramble(vcdu);
		rs_fix_count = rs_fix_packet(self->rs, vcdu, NULL);

		/* Check RS return status */
		if (rs_fix_count < 0) {
			self->next_header = NULL;
			seg->len = 0;
			return -1;
		}

		/* Copy the bytes we were missing */
		memcpy(seg->data + bytes_out, vcdu->mpdu_data, 
		       MIN(MPDU_DATA_SIZE, seg->len - bytes_out));

		/* Huge packet that spans more than two VCDUs... possible? Maybe,
		 * haven't found anything saying this isn't possible in the docs */
		while((self->next_header = vcdu_mpdu_header_ptr(vcdu)) == NULL) {
			bytes_out += MPDU_DATA_SIZE;
			bytes_in = self->src->read(self->src, self->cadu, sizeof(self->cadu));
			if (bytes_in < (int)sizeof(Cadu)) {
				seg->len = 0;
				return 0;
			}

			/* Descramble and error-correct the vcdu */
			descramble(vcdu);
			rs_fix_count = rs_fix_packet(self->rs, vcdu, NULL);

			/* Check RS return status */
			if (rs_fix_count < 0) {
				self->next_header = NULL;
				seg->len = 0;
				return -1;
			}
			memcpy(seg->data + bytes_out, vcdu->mpdu_data, 
			       MIN(MPDU_DATA_SIZE, seg->len - bytes_out));
		}

	} else {
		/* Copy the data from the MPDU and update next_header to point to the
		 * next header in the same VCDU */
		memcpy(seg->data, mpdu_data_ptr(mpdu), seg->len);
		self->next_header = (uint8_t*)mpdu + mpdu_raw_len(mpdu) + 
		                    MPDU_HDR_SIZE + 1;
		if ((uint8_t*)self->next_header >= (uint8_t*)vcdu->mpdu_data + MPDU_DATA_SIZE) {
			self->next_header = NULL;
		}
	}

	return seg->len;
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "packet.h"
#include "packetizer.h"
#include "reedsolomon.h"
#include "source.h"
#include "utils.h"

static void compute_noise(void);
static void descramble(Vcdu *p);
static int  retrieve_and_fix(Cadu *dst, HardSource *src, ReedSolomon *rs);

static uint8_t _noise[NOISE_PERIOD];
static int     _initialized = 0;

Packetizer*
pkt_init(HardSource *src)
{
	Packetizer *ret;

	if (!_initialized) {
		compute_noise();
		_initialized = 1;
	}

	ret = safealloc(sizeof(*ret));
	ret->rs = rs_init(sizeof(Vcdu), INTERLEAVING);
	ret->src = src;
	ret->next_header = NULL;

	return ret;
}

void
pkt_deinit(Packetizer *pp)
{
	if (!pp) {
		return;
	}

	rs_deinit(pp->rs);
	free(pp);
}

/* Retrieve the next segment.
 * "The ugliest function ever written. Period." */
int
pkt_read(Packetizer *self, Segment *seg)
{
	Vcdu *vcdu;
	Mpdu *mpdu, frag_hdr;
	uint8_t *data_ptr;
	int rs_fix_count;
	int bytes_out;
	int timestamp_present;

	vcdu = &(((Cadu*)self->cadu)->cvcdu);

	bytes_out = 0;
	if (!self->next_header) {
		/* Fetch a new CADU */
		rs_fix_count = retrieve_and_fix((Cadu*)self->cadu, self->src, self->rs);

		/* Check RS return status */
		if (rs_fix_count == -1) {
			self->next_header = NULL;
			seg->len = 0;
			return -1;
		} else if (rs_fix_count == -2) {
			seg->len = 0;
			return 0;
		}

		mpdu = vcdu_mpdu_header_ptr(vcdu);
		self->next_header = mpdu;
	} else {
		/* Read from the local VCDU */
		mpdu = (Mpdu*)(self->next_header);
	}

	/* vcdu_mpdu_header_ptr might return NULL, especially when corrupted packets
	 * slip through: handle that case */
	if (!mpdu) {
		self->next_header = NULL;
		seg->len = 0;
		return -1;
	}

	/* Check whether the header is fragmented across multiple VCDUs. If it is,
	 * reconstruct it locally before trying to parse it */
	if ((uint8_t*)mpdu + MPDU_HDR_SIZE > (uint8_t*)vcdu->mpdu_data + MPDU_DATA_SIZE) {
		/* Get the first fragment from the end of the current packet */
		bytes_out = MPDU_DATA_SIZE - ((uint8_t*)mpdu - vcdu->mpdu_data);

		/* Corrupted packets might get here for some reason, so this avoids
		 * segfaults from the upcoming memcpy */
		if (bytes_out < 0 || bytes_out > MPDU_HDR_SIZE) {
			bytes_out = MAX(0, MIN(MPDU_HDR_SIZE, bytes_out));
		}

		memcpy(&frag_hdr, mpdu, bytes_out);

		/* Fetch a new packet to get the second fragment */
		rs_fix_count = retrieve_and_fix((Cadu*)self->cadu, self->src, self->rs);

		if (rs_fix_count == -1) {
			self->next_header = NULL;
			seg->len = 0;
			return -1;
		} else if (rs_fix_count == -2) {
			seg->len = 0;
			return 0;
		}

		/* Append the second fragment to the first */
		memcpy((uint8_t*)&frag_hdr + bytes_out, vcdu->mpdu_data, MPDU_HDR_SIZE - bytes_out);
		self->next_header = vcdu_mpdu_header_ptr(vcdu);

		seg->len = mpdu_data_len(&frag_hdr);
		seg->apid = mpdu_apid(&frag_hdr);
		seg->seq = mpdu_seq(&frag_hdr);
		timestamp_present = mpdu_has_sec_hdr(&frag_hdr);

		/* "Fake" the mpdu beginning  as if it were before the beginning of the
		 * actual mpdu. This is needed because of alignment issues when
		 * computing the next header's position later on */
		data_ptr = (uint8_t*)vcdu->mpdu_data + (MPDU_HDR_SIZE - bytes_out);
		mpdu = (Mpdu*)((uint8_t*)vcdu->mpdu_data - bytes_out);
	} else {
		seg->len = mpdu_data_len(mpdu);
		seg->apid = mpdu_apid(mpdu);
		seg->seq = mpdu_seq(mpdu);
		timestamp_present = mpdu_has_sec_hdr(mpdu);

		data_ptr = mpdu_data_ptr(mpdu);
	}
	seg->timestamp = 0;

	/* This might happen when RS thinks the packet is good but it really isn't */
	if (seg->len > MAX_PKT_SIZE) {
		self->next_header = NULL;
		seg->len = 0;
		return -1;
	}


	/* Check whether the data is fragmented across multiple VCDUs */
	if (data_ptr + seg->len > (uint8_t*)vcdu->mpdu_data + MPDU_DATA_SIZE - 1) {
		/* Packet is fragmented: write what we have and grab a new VCDU */
		bytes_out = MPDU_DATA_SIZE - (data_ptr - vcdu->mpdu_data);
		if (bytes_out > 0) {
			memcpy(seg->data, data_ptr, bytes_out);
		}

		rs_fix_count = retrieve_and_fix((Cadu*)self->cadu, self->src, self->rs);

		/* Check RS return status */
		if (rs_fix_count == -1) {
			self->next_header = NULL;
			seg->len = 0;
			return -1;
		} else if (rs_fix_count == -2) {
			seg->len = 0;
			return 0;
		}

		/* Copy the bytes we were missing */
		data_ptr = vcdu->mpdu_data;
		if (bytes_out < 0) {
			data_ptr -= bytes_out;
			bytes_out = 0;
		}
		memcpy(seg->data + bytes_out, data_ptr, seg->len - bytes_out);
		self->next_header = vcdu_mpdu_header_ptr(vcdu);
	} else {
		/* Copy the data from the MPDU and update next_header to point to the
		 * next header in the current VCDU (or null if that would be past the
		 * end) */
		memcpy(seg->data, data_ptr, seg->len);
		self->next_header = (uint8_t*)mpdu + MPDU_HDR_SIZE + seg->len;
		if ((uint8_t*)self->next_header >= (uint8_t*)vcdu->mpdu_data + MPDU_DATA_SIZE) {
			self->next_header = NULL;
		}
	}

	/* If the timestamp is present, parse it and remove it from the data we'll
	 * be returning */
	if (timestamp_present) {
		seg->timestamp = mpdu_msec((Timestamp*)seg->data);
		seg->len -= MPDU_SEC_HDR_SIZE;
		memmove(seg->data, seg->data + MPDU_SEC_HDR_SIZE, seg->len);
	}

	return seg->len;
}

/* Static functions {{{*/
/* Byte by byte, xor the packet with the added noise */
static int
retrieve_and_fix(Cadu *dst, HardSource *src, ReedSolomon *rs)
{
	int bytes_in;
	int rs_fix_count;

	bytes_in = src->read(src, (uint8_t*)dst, sizeof(*dst));
	if (bytes_in < (int)sizeof(*dst)) {
		/* EOF reached */
		return -2;
	}

	/* Descramble and error-correct the vcdu */
	descramble(&dst->cvcdu);
	rs_fix_count = rs_fix_packet(rs, &dst->cvcdu);

	if(rs_fix_count < 0) {
		/* Unfixable frame */
		return -1;
	}
	return rs_fix_count;
}

/* XOR with the precomputed output of the LFSR */
static void
descramble(Vcdu *p)
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

#include <stdint.h>
#include <string.h>
#include "channel.h"
#include "correlator/autocorrelator.h"
#include "correlator/correlator.h"
#include "decode.h"
#include "diffcode/diffcode.h"
#include "deinterleave/deinterleave.h"
#include "ecc/descramble.h"
#include "ecc/rs.h"
#include "ecc/viterbi.h"
#include "protocol/cadu.h"
#include "parser/mpdu_parser.h"
#include "parser/mcu_parser.h"
#include "utils.h"

static int read_samples(int (*read)(int8_t *dst, size_t len), int8_t *dst, size_t len);

static int _rs, _vit;
static uint32_t _vcdu_seq;
static int _diffcoded;
static int _interleaved;
static enum { READ, PARSE_MPDU, VIT_SECOND } _state;

#ifndef NDEBUG
FILE *_vcdu;
#endif


void
decode_init(int diffcoded, int interleaved)
{
	uint64_t convolved_syncword;

	/* Initialize subsystems */
	conv_encode_u32(&convolved_syncword, 0, SYNCWORD);
	correlator_init(convolved_syncword);
	viterbi_init();
	descramble_init();
	rs_init();
	mpdu_parser_init();

#ifndef NDEBUG
	_vcdu = fopen("/tmp/vcdu.data", "wb");
#endif

	_rs = 0;
	_vit = 0;
	_vcdu_seq = 0;
	_diffcoded = diffcoded;
	_interleaved = interleaved;
	_state = READ;
}

DecoderState
decode_soft_cadu(Mpdu *dst, int (*read)(int8_t *dst, size_t len))
{
	static int8_t soft_cadu[INTER_SIZE(2*CADU_SOFT_LEN)];
	static int offset;
	static int vit;
	static Cadu cadu;

	uint8_t hard_cadu[CONV_CADU_LEN];
	int errors;
	int i;
	enum phase rotation;

	switch (_state) {
		case READ:
			/* Read a CADU worth of samples */
			for (i=0; i<CADU_SOFT_LEN; i+=CADU_SOFT_CHUNK) {
				if (read_samples(read, soft_cadu+i, CADU_SOFT_CHUNK)) {
					return EOF_REACHED;
				}
			}

			/* Differentially decode if necessary */
			if (_diffcoded) diff_decode(soft_cadu, CADU_SOFT_LEN);

			/* Perform correlation and advance to the next state */
			soft_to_hard(hard_cadu, soft_cadu, CADU_SOFT_LEN);
			offset = correlate(&rotation, hard_cadu, CONV_CADU_LEN);

			/* Read more samples to get a full CADU */
			if (offset && read_samples(read, soft_cadu+CADU_SOFT_LEN, offset))
				return EOF_REACHED;

			/* Derotate */
			soft_derotate(soft_cadu+offset, CADU_SOFT_LEN, rotation);

			/* Finish decoding the past frame (output is VITERBI_DELAY bits late) */
			vit = viterbi_decode(((uint8_t*)&cadu) + sizeof(Cadu)-VITERBI_DELAY,
					soft_cadu+offset,
					VITERBI_DELAY);

			/* Descramble and error correct */
			descramble(&cadu);
			errors = rs_fix(&cadu.data);
			_rs = errors;
#ifndef NDEBUG
			fwrite(&cadu, sizeof(cadu), 1, _vcdu);
			fflush(_vcdu);
#endif

			/* If RS reports failure, reinitialize the internal state of the
			 * MPDU decoder and finish up the Viterbi decode process. */
			if (errors < 0) {
				mpdu_parser_init();
				_state = VIT_SECOND;
				break;
			}

			_vcdu_seq = vcdu_counter(&cadu.data);
			_state = PARSE_MPDU;
			__attribute__((fallthrough));
		case PARSE_MPDU:
			/* Parse the next MPDU in the decoded VCDU */
			switch (mpdu_reconstruct(dst, &cadu.data)) {
				case PARSED:
					return MPDU_READY;
				case PROCEED:
					_state = VIT_SECOND;
					break;
				default:
					break;
			}
			break;

		case VIT_SECOND:
			/* Viterbi decode (2/2) */
			vit += viterbi_decode((uint8_t*)&cadu,
					soft_cadu+offset+2*8*VITERBI_DELAY,
					sizeof(Cadu)-VITERBI_DELAY);
			_vit = vit / sizeof(Cadu);
			_state = READ;
			return STATS_ONLY;
			break;

		default:
			_state = READ;
			break;

	}

	return NOT_READY;
}

int
decode_get_rs()
{
	return _rs;
}

int
decode_get_vit()
{
	return _vit;
}

uint32_t
decode_get_vcdu_seq()
{
	return _vcdu_seq;
}

static int
read_samples(int (*read)(int8_t *dst, size_t len), int8_t *dst, size_t len)
{
	static int offset;
	static int8_t from_prev[INTER_MARKER_STRIDE];
	int deint_offset;
	int num_samples;
	uint8_t hard[INTER_SIZE(len)];
	static enum phase rotation;

	/* If not interleaved, directly read and return */
	if (!_interleaved) return !read(dst, len);

	/* Retrieve enough samples so that the deinterleaver will output
	 * $len samples. Use the internal cache first */
	num_samples = deinterleave_num_samples(len);
	if (offset) {
		memcpy(dst, from_prev, MIN(offset, num_samples));
		memcpy(from_prev, from_prev+offset, offset-MIN(offset, num_samples));
	}
	if (num_samples-offset > 0 && !read(dst+offset, num_samples-offset)) return 1;
	offset -= MIN(offset, num_samples);

	if (num_samples < INTER_MARKER_STRIDE*8) {
		/* Not enough bytes to reliably find sync marker offset: assume the
		 * offset is correct, and just derotate and deinterleave what we read */
		soft_derotate(dst, num_samples, rotation);
		deinterleave(dst, dst, len);
	} else {
		/* Find synchronization marker (offset with the best autocorrelation) */
		soft_to_hard(hard, dst, num_samples & ~0x7);
		offset = autocorrelate(&rotation, INTER_MARKER_STRIDE/8, hard, num_samples/8);

		/* Get where the deinterleaver expects the next marker to be */
		deint_offset = deinterleave_expected_sync_offset();

		/* Compute the delta between the expected marker position and the
		 * one found by the correlator */
		offset = (offset - deint_offset + INTER_MARKER_INTERSAMPS + 1) % INTER_MARKER_STRIDE;
		offset = offset > INTER_MARKER_STRIDE/2 ? offset-INTER_MARKER_STRIDE : offset;

		/* If the offset is positive, read more
		 * bits to get $num_samples valid samples. If the offset is negative,
		 * copy the last few bytes into the local cache */
		if (offset > 0) {
			if (!read(dst+num_samples, offset)) return 1;
		} else {
			memcpy(from_prev, dst+num_samples+offset, -offset);
		}

		/* Correct rotation for these samples */
		soft_derotate(dst, num_samples+offset, rotation);

		/* Deinterleave */
		deinterleave(dst, dst+offset, len);
		offset = offset < 0 ? -offset : 0;
	}

	return 0;
}

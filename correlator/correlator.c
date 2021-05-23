#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "correlator.h"
#include "ecc/viterbi.h"
#include "utils.h"

#define ROTATIONS 4

static uint32_t hard_rotate_u32(uint32_t word, enum phase amount);
static inline int correlate_u64(uint64_t x, uint64_t y);

static uint64_t _syncwords[ROTATIONS];

void
correlator_init(uint64_t syncword)
{
	int i;

	for (i=0; i<ROTATIONS; i++) {
		_syncwords[i] = (uint64_t)hard_rotate_u32(syncword >> 32, i) << 32 |
		                          hard_rotate_u32(syncword & 0xFFFFFFFF, i);
		_syncwords[i] = ((_syncwords[i] & 0x5555555555555555) << 1)
		              | ((_syncwords[i] & 0xAAAAAAAAAAAAAAAA) >> 1);
	}
}


int
correlate(enum phase *restrict best_phase, uint8_t *restrict hard_cadu, int len)
{
	enum phase phase;
	int corr, best_corr, best_offset;
	int i, j;
	uint64_t window;
	uint8_t tmp;

	best_corr = 0;
	best_offset = 0;
	*best_phase = PHASE_0;

	window = ((uint64_t)hard_cadu[0] << 56) |
		((uint64_t)hard_cadu[1] << 48) |
		((uint64_t)hard_cadu[2] << 40) |
		((uint64_t)hard_cadu[3] << 32) |
		((uint64_t)hard_cadu[4] << 24) |
		((uint64_t)hard_cadu[5] << 16) |
		((uint64_t)hard_cadu[6] << 8) |
		((uint64_t)hard_cadu[7] << 0);
	hard_cadu += 8;

	/* Prioritize offset 0 */
	for (phase=PHASE_0; phase<=PHASE_270; phase++) {
		if (correlate_u64(_syncwords[phase], window) > CORR_THR) {
			*best_phase = phase;
			return 0;
		}
	}

	/* For each byte in the CADU */
	for (i=0; i<len-8; i++) {
		/* Fetch a byte from the CADU */
		tmp = *hard_cadu++;

		/* For each bit in the byte (can't do pairs because OQPSK symbols may be
		 * offset by one rather than two) */
		for (j=0; j<8; j++) {

			/* Take all possible rotations of the syncword into account */
			for (phase=PHASE_0; phase<=PHASE_270; phase++) {
				corr = correlate_u64(_syncwords[phase], window);
				if (corr > best_corr) {
					best_corr = corr;
					best_offset = i*8 + j;
					*best_phase = phase;
				}
			}

			/* Advance window by one */
			window = (window << 1) | ((tmp >> (7-j)) & 0x1);
		}
	}

	return best_offset;
}


/* Static functions {{{ */
static uint32_t
hard_rotate_u32(uint32_t word, enum phase amount)
{
	const uint32_t i = word & 0xaaaaaaaa;
	const uint32_t q = word & 0x55555555;

	switch (amount) {
		case PHASE_0:
			break;
		case PHASE_90:
			word = ((i ^ 0xaaaaaaaa) >> 1) | (q << 1);
			break;
		case PHASE_180:
			word = word ^ 0xffffffff;
			break;
		case PHASE_270:
			word = (i >> 1) | ((q ^ 0x55555555) << 1);
			break;
		default:
			break;
	}

	return word;
}

static inline int
correlate_u64(uint64_t x, uint64_t y)
{
	int corr;
	uint64_t v = x ^ y;

	for (corr = 0; v; corr++) {
		v &= v-1;
	}

	return 64 - corr;
}
/* }}} */

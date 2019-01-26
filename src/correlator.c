#include <stdint.h>
#include <stdlib.h>
#include "correlator.h"
#include "lrpt.h"
#include "utils.h"
#include "viterbi.h"

typedef enum {
	PHASE_90,
	PHASE_180,
	PHASE_270,
} Phase;

static int  correlate(int8_t x, uint8_t hard);
static void add_pattern(Correlator *self, uint8_t *pattern);
static void iq_rotate(uint8_t *buf, size_t count, Phase p);
static void iq_rotate_soft(int8_t *buf, size_t count, Phase p);
static void iq_reverse(uint8_t *buf, size_t count);
static void iq_reverse_soft(int8_t *buf, size_t count);
static int  qw_correlate(int8_t *soft, uint8_t *hard);

/* 90 degree phase shift + phase mirroring lookup table 
 * XXX HARD BYTES, NOT SOFT SAMPLES XXX
 */
int8_t _rot_lut[256];
int8_t _inv_lut[256];


void
correlator_deinit(Correlator *c)
{
	free(c->patterns);
	free(c);
}


/* Initialize the correlator */
Correlator*
correlator_init(const uint8_t syncword[4])
{
	int i;
	uint8_t encoded_syncword[8];
	Correlator *c;

	c = safealloc(sizeof(*c));
	c->pattern_count = 0;
	c->patterns = NULL;
	c->active_correction = 0;

	for (i=0; i<255; i++) {
		_rot_lut[i] = ((i & 0x55) ^ 0x55) << 1 | (i & 0xAA) >> 1;
		_inv_lut[i] = (i & 0x55) << 1 | (i & 0xAA) >> 1;
	}

	/* Find the coded version of the pattern */
	viterbi_encode(syncword, 4, encoded_syncword);

	/* Correlate for all 8 possible rotations of the syncword */
	for (i=0; i<4; i++) {
		add_pattern(c, encoded_syncword);
		iq_rotate(encoded_syncword, 8, PHASE_90);
	}
	iq_reverse(encoded_syncword, 8);
	for (i=0; i<4; i++) {
		add_pattern(c, encoded_syncword);
		iq_rotate(encoded_syncword, 8, PHASE_90);
	}

	return c;
}

/* Correct a frame based on the pattern we locked on to earlier */
void
correlator_soft_fix(Correlator *self, int8_t* frame, size_t size)
{
	int to_apply;

	to_apply = self->active_correction;
	if (to_apply > 3) {
		to_apply -= 4;
		iq_reverse_soft(frame, size);
	}

	switch(to_apply) {
	case 0:
		break;
	case 1:
		iq_rotate_soft(frame, size, PHASE_90);
		break;
	case 2:
		iq_rotate_soft(frame, size, PHASE_180);
		break;
	case 3:
		iq_rotate_soft(frame, size, PHASE_270);
		break;
	default:
		break;
	}
}

/* Try to find one of the patterns inside a frame */
int
correlator_soft_correlate(Correlator *self, int8_t *frame, size_t len)
{
	size_t i, pattern;
	int max_corr, max_corr_pos, tmp_corr;
	int *correlation, *corr_pos;

	/* Save the correlation indices and their correlation factor */
	correlation = calloc(1, sizeof(*correlation) * self->pattern_count);
	corr_pos = calloc(1, sizeof(*corr_pos) * self->pattern_count);

	/* Compute the correlation indices for each position, for each pattern, up
	 * until there are no qwords left (so 64 bytes early) */
	for (i=0; i<len-64; i++) {
		for (pattern=0; pattern<self->pattern_count; pattern++) {
			tmp_corr = qw_correlate(frame+i, self->patterns[pattern]);

			/* Fast exit in case correlation is very high */
			if (tmp_corr > CORRELATION_THR) {
				max_corr_pos = i;
				goto cleanup;
			}

			if (tmp_corr > correlation[pattern]) {
				correlation[pattern] = tmp_corr;
				corr_pos[pattern] = i;
			}
		}
	}

	/* Find the position with the highest correlation */
	max_corr = correlation[0];
	max_corr_pos = corr_pos[0];
	for (pattern=1; pattern<self->pattern_count; pattern++) {
		if (correlation[pattern] > max_corr) {
			max_corr = correlation[pattern];
			max_corr_pos = corr_pos[pattern];
			self->active_correction = pattern;
		}
	}

cleanup:
	free(correlation);
	free(corr_pos);

	return max_corr_pos;
}

/* Static functions {{{*/
/* Add a pattern to the list we'll be looking for */
static void
add_pattern(Correlator *self, uint8_t *pattern)
{
	int i;

	self->pattern_count++;
	self->patterns = realloc(self->patterns,
	                           sizeof(*self->patterns) * self->pattern_count);

	for (i=0; i<8; i++) {
		self->patterns[self->pattern_count-1][i] = pattern[i];
	}
}

/* Rotate a bit pattern 90 degrees in phase */
static void
iq_rotate(uint8_t *buf, size_t count, Phase p)
{
	size_t i;

	for (i=0; i<count; i++) {
		if (p == PHASE_90 || p == PHASE_270) {
			buf[i] = _rot_lut[buf[i]];
		}
		if (p == PHASE_180 || p == PHASE_270) {
			buf[i] ^= 0xFF;
		}
	}
}

static void
iq_rotate_soft(int8_t *buf, size_t count, Phase p)
{
	size_t i;
	int8_t tmp;

	switch(p) {
	case PHASE_90:
		for (i=0; i<count; i+=2) {
			tmp = buf[i];
			buf[i] = -buf[i+1];
			buf[i+1] = tmp;
		}
		break;
	case PHASE_180:
		for (i=0; i<count; i++) {
			buf[i] = -buf[i];
		}
		break;
	case PHASE_270:
		for (i=0; i<count; i+=2) {
			tmp = buf[i];
			buf[i] = buf[i+1];
			buf[i+1] = -tmp;
		}
		break;
	default:
		break;
	}
}
			
/* Flip I<->Q in a bit pattern */
static void
iq_reverse(uint8_t *buf, size_t count)
{
	size_t i;
	
	for (i=0; i<count; i++) {
		buf[i] = _inv_lut[buf[i]];
	}
}

static void
iq_reverse_soft(int8_t *buf, size_t count)
{
	size_t i;
	int8_t tmp;

	for (i=0; i<count; i+=2) {
		tmp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = tmp;
	}
}

/* Correlate a soft sample and a hard sample */
static int
correlate(int8_t x, uint8_t hard)
{
	return !((x < 0) ^ !(hard & 0x01));
}


/* Correlate a soft qword and a hard qword */
static int
qw_correlate(int8_t *soft, uint8_t *hard)
{
	int i, j, correlation;

	correlation = 0;

	/* 1 qword = 8 bytes = 32 symbols */
	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			correlation += correlate(soft[i*8+j], hard[i] >> (7-j));
			if (correlation > CORRELATION_THR) {
				return correlation;
			}
		}
	}

	return correlation;
}

/*}}}*/

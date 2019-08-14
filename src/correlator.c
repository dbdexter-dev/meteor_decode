#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "correlator.h"
#include "packet.h"
#include "source.h"
#include "utils.h"
#include "viterbi.h"

typedef struct {
	uint8_t (*patterns)[64];
	size_t pattern_count;
	int active_correction;
	int8_t buffer[SOFT_FRAME_SIZE * 2];
	int buffer_offset, frame_offset;
	SoftSource *src;
} Correlator;

static int  correlator_read_aligned(SoftSource *src, int8_t *out, size_t count);
static void correlator_deinit_soft(SoftSource *c);
static int  correlator_soft_correlate(Correlator *c, const int8_t *frame, size_t len);
static void correlator_soft_fix(Correlator *c, int8_t* frame, size_t len);

static void soft_to_hard(uint8_t *dst, const int8_t *src, size_t nbytes);
static void add_pattern(Correlator *self, const uint8_t *pattern);
static int  qw_correlate(const uint8_t *soft, const uint8_t *hard);


/* Initialize the correlator */
SoftSource*
correlator_init_soft(SoftSource *src, uint8_t syncword[PATT_SIZE])
{
	int i;
	Correlator *c;
	SoftSource *ret;

	ret = safealloc(sizeof(*ret));
	ret->read = correlator_read_aligned;
	ret->close = correlator_deinit_soft;

	c = safealloc(sizeof(*c));
	c->pattern_count = 0;
	c->patterns = NULL;
	c->active_correction = 0;
	c->buffer_offset = 0;
	c->src = src;

	/* Correlate for all 8 possible rotations of the syncword */
	for (i=0; i<4; i++) {
		add_pattern(c, syncword);
		iq_rotate_hard(syncword, PATT_SIZE, PHASE_90);
	}
	iq_reverse_hard(syncword, PATT_SIZE);
	for (i=0; i<4; i++) {
		add_pattern(c, syncword);
		iq_rotate_hard(syncword, PATT_SIZE, PHASE_90);
	}

	ret->_backend = c;

	return ret;
}

void
correlator_deinit_soft(SoftSource *src)
{
	Correlator *c = src->_backend;

	free(c->patterns);
	free(c);
	free(src);
}

/* Ladies and gentlemen, I present to you:
 * "The ugliest function ever written II: Electric Boogaloo"
 */
int
correlator_read_aligned(SoftSource *src, int8_t *out, size_t count)
{
	size_t bytes_in, bytes_out;
	Correlator *const self = src->_backend;

	bytes_out = 0;
	/* Might have some bytes left from the last read: copy them in the new
	 * buffer */
	if (self->buffer_offset) {
		bytes_out = MIN(count, SOFT_FRAME_SIZE - self->buffer_offset);
		memcpy(out, self->buffer+self->frame_offset+self->buffer_offset, bytes_out);

		self->buffer_offset = (self->buffer_offset+bytes_out)%SOFT_FRAME_SIZE;
		out += bytes_out;
		count -= bytes_out;
	}

	while (count > 0) {
		/* Attempt to read a frame into the local buffer */
		bytes_in = self->src->read(self->src, self->buffer, SOFT_FRAME_SIZE);

		/* If there aren't enough bytes left, fix the frame and return */
		if (bytes_in < (int)SOFT_FRAME_SIZE) {
			correlator_soft_fix(self, self->buffer, bytes_in);
			memcpy(out, self->buffer, MIN(bytes_in, count));
			if (count < bytes_in) {
				self->buffer_offset = count;
			}
			return bytes_out + MIN(bytes_in, count);
		}

		/* Check how far we were from the actual frame */
		self->frame_offset = correlator_soft_correlate(self, self->buffer, SOFT_FRAME_SIZE);
		/* Read some more bytes to compensate for the offset */
		bytes_in = self->src->read(self->src, self->buffer+SOFT_FRAME_SIZE, self->frame_offset);
		/* Fix the sample's phase */
		correlator_soft_fix(self, self->buffer + self->frame_offset, SOFT_FRAME_SIZE);

		/* If there aren't enough bytes left, return */
		if ((int)bytes_in < self->frame_offset) {
			return bytes_out;
		}

		/* TODO: this might copy some garbage at the end, but whatever, it would
		 * only be for the very last iteration. I'll fix it at some point :P */
		if (count < SOFT_FRAME_SIZE) {
			memcpy(out, self->buffer+self->frame_offset, count);
			self->buffer_offset = count;
			return bytes_out + count;
		}

		memcpy(out, self->buffer+self->frame_offset, SOFT_FRAME_SIZE);

		out += SOFT_FRAME_SIZE;
		bytes_out += SOFT_FRAME_SIZE;
		count -= SOFT_FRAME_SIZE;
	}

	return bytes_out;
}


/* Try to locate one of the patterns inside a frame */
int
correlator_soft_correlate(Correlator *self, const int8_t *frame, size_t len)
{
	size_t i, pattern;
	int max_corr, max_corr_pos, tmp_corr;
	int *correlation, *corr_pos;
	uint8_t *hard_frame;

	/* Save the correlation indices and their correlation factor */
	correlation = calloc(self->pattern_count, sizeof(*correlation));
	corr_pos = calloc(self->pattern_count, sizeof(*corr_pos));
	hard_frame = safealloc(len);

	/* Convert the soft frame to a hard one */
	soft_to_hard(hard_frame, frame, len);

	/* Prioritize the correlation in the first position */
	for (pattern=0; pattern<self->pattern_count; pattern++) {
		tmp_corr = qw_correlate(hard_frame, self->patterns[pattern]);
		if (tmp_corr >= CORRELATION_THR - 10) {
			self->active_correction = pattern;
			free(correlation);
			free(corr_pos);
			free(hard_frame);
			return 0;
		}
	}

	/* Compute the correlation indices for each position, for each pattern, up
	 * until there are no qwords left (so 64 bytes early) */
	for (i=1; i<len-64; i++) {
		for (pattern=0; pattern<self->pattern_count; pattern++) {
			tmp_corr = qw_correlate(hard_frame+i, self->patterns[pattern]);

			/* Fast exit in case correlation is very high */
			if (tmp_corr >= CORRELATION_THR) {
				self->active_correction = pattern;
				max_corr_pos = i;
				free(correlation);
				free(corr_pos);
				free(hard_frame);
				return i;
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

	free(hard_frame);
	free(correlation);
	free(corr_pos);
	return max_corr_pos;
}

/* Correct a frame based on the pattern we locked on to earlier */
void
correlator_soft_fix(Correlator *self, int8_t* frame, size_t len)
{
	int to_apply;

	to_apply = self->active_correction;
	if (to_apply > 3) {
		to_apply -= 4;
		iq_reverse_soft(frame, len);
	}

	switch(to_apply) {
	case 0:
		break;
	case 1:
		iq_rotate_soft(frame, len, PHASE_90);
		break;
	case 2:
		iq_rotate_soft(frame, len, PHASE_180);
		break;
	case 3:
		iq_rotate_soft(frame, len, PHASE_270);
		break;
	default:
		break;
	}
}

/* Static functions {{{*/
/* Add a pattern to the list we'll be looking for */
static void
add_pattern(Correlator *self, const uint8_t *pattern)
{
	self->pattern_count++;
	self->patterns = realloc(self->patterns,
	                           sizeof(*self->patterns) * self->pattern_count);

	bit_expand(self->patterns[self->pattern_count-1], pattern, PATT_SIZE);
}


/* Convert soft samples into a byte-expanded bit representation */
static void
soft_to_hard(uint8_t *hard, const int8_t *soft, size_t nbytes)
{
	for (nbytes--; nbytes>0; nbytes--) {
		*hard++ = (*soft++ > 0);
	}
}

/* Correlate two bit-expanded qwords */
static int
qw_correlate(const uint8_t *soft, const uint8_t *hard)
{
	int i, correlation;

	correlation = 0;

	/* 1 qword = 8 bytes = 32 symbols = 64 samples */
	for (i=0; i<64; i++) {
		correlation += !(*soft++ ^ *hard++);
	}

	return correlation;
}
/*}}}*/

#include <assert.h>
#include <stdint.h>
#include "deinterleave.h"

static int8_t _deint[INTER_BRANCH_COUNT * INTER_BRANCH_COUNT * INTER_BRANCH_DELAY];
static int _cur_branch = 0;
static int _offset = 0;

void
deinterleave(int8_t *dst, const int8_t *src, size_t len)
{
	int delay, write_idx, read_idx;
	size_t i;

	read_idx = (_offset + INTER_BRANCH_COUNT*INTER_BRANCH_DELAY) % sizeof(_deint);
	assert(len < sizeof(_deint));

	/* Write bits to the deinterleaver */
	for (i=0; i<len; i++) {

		/* Skip sync marker */
		if (!_cur_branch) {
			src += 8;
		}

		/* Compute the delay of the current symbol based on the branch we're on */
		delay = (_cur_branch % INTER_BRANCH_COUNT) * INTER_BRANCH_DELAY * INTER_BRANCH_COUNT;
		write_idx = (_offset - delay + sizeof(_deint)) % sizeof(_deint);

		_deint[write_idx] = *src++;

		_offset = (_offset + 1) % sizeof(_deint);
		_cur_branch = (_cur_branch + 1) % INTER_MARKER_INTERSAMPS;
	}

	/* Read bits from the deinterleaver */
	for (; len>0; len--) {
		*dst++ = _deint[read_idx];
		read_idx = (read_idx + 1) % sizeof(_deint);
	}
}

size_t
deinterleave_num_samples(size_t output_count)
{
	int num_syncs;

	if (!output_count) return 0;

	num_syncs = (_cur_branch ? 0 : 1)
	          + (output_count - (INTER_MARKER_INTERSAMPS - _cur_branch) + INTER_MARKER_INTERSAMPS-1)
	          / INTER_MARKER_INTERSAMPS;

	return output_count + 8*num_syncs;
}

int
deinterleave_expected_sync_offset()
{
	return _cur_branch ? INTER_MARKER_INTERSAMPS - _cur_branch : 0;
}

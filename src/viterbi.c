#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "source.h"
#include "utils.h"
#include "viterbi.h"

typedef struct {
	uint8_t output;
	unsigned int next_state;
} Transition;

typedef struct state {
	unsigned int cost;
	uint8_t data[MEM_DEPTH+1];
} Path;

typedef struct {
	int cur_depth;
	Transition trans[N_STATES][2];
	Path (*mem)[N_STATES];
	Path (*tmp)[N_STATES];
	SoftSource *src;
} Viterbi;

static void compute_trans(Viterbi *v);
static int  find_best(const Viterbi *self, int8_t x, int8_t y, Path *end_state, int id);
static int  parity(uint8_t x);
static int  write_bits(const uint8_t *bits, size_t count, uint8_t *out);
static int  viterbi_deinit(HardSource *v);
static int  viterbi_decode(HardSource *v, uint8_t *out, size_t count);
static int  viterbi_flush(HardSource *v, uint8_t *out, size_t maxlen);

static unsigned int cost(int8_t x, int8_t y, int coding);

static unsigned int _cost_lut[256][3];
static int _initialized = 0;

HardSource*
viterbi_init(SoftSource *src)
{
	int i;
	HardSource *ret;
	Viterbi *v;

	ret = safealloc(sizeof(*ret));
	ret->read = viterbi_decode;
	ret->close = viterbi_deinit;

	v = safealloc(sizeof(*v));
	v->src = src;
	v->cur_depth = 0;

	v->mem = calloc(N_STATES, sizeof(*v->mem));
	v->tmp = calloc(N_STATES, sizeof(*v->tmp));

	if (!_initialized) {
		for (i=-128; i<128; i++) {
			_cost_lut[(uint8_t)i][0] = i + 128;
			_cost_lut[(uint8_t)i][1] = abs(i-127);
			_cost_lut[(uint8_t)i][2] = abs(i-127);
		}
	}

	ret->_backend = v;

	/* Compute the transition matrix */
	compute_trans(v);

	return ret;
}

static int
viterbi_deinit(HardSource *src)
{
	Viterbi *v = src->_backend;

	free(v->mem);
	free(v->tmp);
	free(v);
	free(src);

	return 0;
}

int
viterbi_decode(HardSource *src, uint8_t *out, size_t len)
{
	int fwd_depth;
	int in_pos, points_in, bytes_out;
	int i, count, cur_state;
	int8_t in[2*MEM_DEPTH];
	unsigned int mincost;
	int8_t x, y;
	Path *best, (*tmp)[N_STATES];
	Viterbi *self = src->_backend;

	/* Len must be a multiple of 8 */
	assert(!(len & 0x07));

	bytes_out = 0;

	/* Repeat until $len bytes have been written out, or until we exhausted the
	 * source we get the data from */
	while ((int)len > bytes_out) {
		/* Calculate how deep to go and how many bytes to read */
		fwd_depth = (size_t)(MEM_DEPTH - self->cur_depth);
		points_in = self->src->read(self->src, in, 2*fwd_depth)/2;
		if (points_in < fwd_depth) {
			/* We reached the end of the source: just flush the Viterbi memory
			 * instead of doing the whole algorithm */
			return viterbi_flush(src, out, len);
		}
		fwd_depth = points_in;

		/* Run the Viterbi algorithm forward */
		for (i=0, in_pos=0; i<(int)fwd_depth; i++) {
			/* Get a symbol from the source */
			x = in[in_pos++];
			y = in[in_pos++];

			/* For each state, find the path of least resistance to it */
			for (cur_state = 0; cur_state < N_STATES; cur_state++) {
				find_best(self, x, y, self->tmp[cur_state], cur_state);
			}

			/* Update the Viterbi decoder memory (swap tmp and mem) */
			tmp = self->mem;
			self->mem = self->tmp;
			self->tmp = tmp;

			self->cur_depth++;
		}

		/* Find the best path so far */
		best = self->mem[0];
		for (i=1; i<N_STATES; i++) {
			if (self->mem[i]->cost < best->cost) {
				best = self->mem[i];
			}
		}

		/* Write out depth - BACKTRACK_DEPTH bits */
		if (self->cur_depth > (int)BACKTRACK_DEPTH) {
			count = self->cur_depth - BACKTRACK_DEPTH;
			i = write_bits(best->data, count, out + bytes_out);
			bytes_out += i;

			/* Realign data and normalize the costs */
			mincost = best->cost;
			for (i=0; i<N_STATES; i++) {
				self->mem[i]->cost -= mincost;
				memmove(self->mem[i]->data, self->mem[i]->data + count, BACKTRACK_DEPTH);
			}
			self->cur_depth = BACKTRACK_DEPTH;
		}
	}

	return bytes_out;
}

/* Finishing step of the Viterbi algorithm: don't keep any nodes for future
 * computations, just return the most likely ending sequence */
int
viterbi_flush(HardSource *v, uint8_t *out, size_t maxlen)
{
	int i;
	Path *best;
	Viterbi *self = v->_backend;

	if (self->cur_depth <= 0) {
		return 0;
	}

	/* Find the best candidate */
	best = self->mem[0];
	for (i=1; i<N_STATES; i++) {
		if (self->mem[i]->cost < best->cost) {
			best = self->mem[i];
		}
	}

	/* Write out BACKTRACK_DEPTH bits */
	i = write_bits(best->data, MIN(maxlen, (size_t)self->cur_depth), out);

	self->cur_depth = 0;
	return i;
}

/* Not really a part of the Viterbi algorithm, but still fits within this file.
 * Byte-wise convolutional encoder, same parameters as the decoder (G1,G2,N,K) */
int
viterbi_encode(uint8_t *out, const uint8_t *in, size_t len)
{
	unsigned int state;
	int i, bit_idx;
	uint8_t out_tmp;

	state = 0;

	for (i=0; i<(int)len; i++) {
		out[0] = 0;
		out[1] = 0;
		for (bit_idx=7; bit_idx >= 0; bit_idx--) {
			/* Compute new state */
			state = ((state >> 1) | (*in >> bit_idx & 0x01) << (K-1)) & (N_STATES-1);

			/* Compute output bits */
			out_tmp = parity(state & G1) << 1 | parity(state & G2);
			out[(bit_idx > 3 ? 0 : 1)] |= out_tmp << ((bit_idx & 0x03) << 1);
		}
		in++;
		out += 2;
	}

	return 2*len;
}

/* Static functions {{{*/
/* Given an end state, find the previous state that gets to it with the least
 * effort */
static int
find_best(const Viterbi *const self, int8_t x, int8_t y, Path *end_state, int id)
{
	int start_state, prev;
	uint8_t input;
	unsigned int tmpcost, mincost;

	mincost = (unsigned int) -1;

	/* Compute the input necessary to get to end_state */
	input = id >> (K-1);

	/* Try with candidate #1 */
	start_state = ((id << 1) & (N_STATES - 1)) | 0x00;
	tmpcost = cost(x, y, self->trans[start_state][input].output);
	if (self->mem[start_state]->cost + tmpcost < MAX_COST) {
		mincost = self->mem[start_state]->cost + tmpcost;
		prev = start_state;
	}

	/* Try with candidate #2 */
	start_state = ((id << 1) & (N_STATES - 1)) | 0x01;
	tmpcost = cost(x, y, self->trans[start_state][input].output);
	if (self->mem[start_state]->cost + tmpcost < mincost) {
		mincost = self->mem[start_state]->cost + tmpcost;
		prev = start_state;
	}

	/* If an ancestor was found, copy its data and fix the metadata */
	/* NOTE: self->cur_depth is a count, not an index. It's index+1 */
	if (mincost < MAX_COST) {
		memcpy(end_state->data, self->mem[prev]->data, self->cur_depth);
		end_state->data[self->cur_depth] = input;
		end_state->cost = mincost;
	} else {
		end_state->cost = (unsigned int) -1;
	}

	return 0;
}

/* Precompute the transition function */
static void
compute_trans(Viterbi *v)
{
	int state, input;
	int output, next;

	for (state=0; state<N_STATES; state++) {
		for (input=0; input<2; input++) {
			next = (state >> 1) | ((input & 0x01) << (K-1));
			output = parity(next & G1) << 1 | parity(next & G2);

			v->trans[state][input].output = output;
			v->trans[state][input].next_state = next;
		}
	}

}

/* Cost function */
static unsigned int
cost(int8_t x, int8_t y, int coding)
{
	return _cost_lut[(uint8_t)x][coding & 0x02] +
	       _cost_lut[(uint8_t)y][coding & 0x01];
}

static int
parity(uint8_t x)
{
	int i, ret;

	for (i=0, ret=0; i<7; i++) {
		ret += (x >> i) & 0x01;
	}

	return ret & 0x01;

}

/* From a uint8_t array of single bits to a compact uint8_t array of bytes */
static int
write_bits(const uint8_t *bits, size_t count, uint8_t *out)
{
	size_t i;
	int bytes_out;
	uint8_t accum;

	assert(!(count & 0x7));

	bytes_out = 0;

	accum = bits[0] << 7;
	for(i=1; i<count; i++) {
		if (!(i%8)) {
			*out++ = accum;
			accum = 0x00;
			bytes_out++;
		}
		accum |= bits[i] << (7 - (i&0x07));
	}
	*out = accum;
	bytes_out++;

	return bytes_out;
}
/*}}}*/

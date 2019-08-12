#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "source.h"
#include "utils.h"
#include "viterbi.h"

typedef struct state {
	uint16_t cost;
	uint8_t data[MEM_DEPTH+1];
} Node;

typedef struct {
	int cur_depth;
	uint8_t outputs[N_STATES][2];
	Node (*mem)[N_STATES];
	Node (*tmp)[N_STATES];
	SoftSource *src;
} Viterbi;

static void compute_trans(Viterbi *v);
static int  parity(uint8_t x);
static inline void update_costs(const Viterbi *self, int8_t x, int8_t y);
static void viterbi_deinit(HardSource *v);
static int  viterbi_decode(HardSource *v, uint8_t *out, size_t count);
static int  viterbi_flush(HardSource *v, uint8_t *out, size_t maxlen);
static int  write_bits(uint8_t *out, const uint8_t *bits, size_t count);

static inline unsigned int cost(int8_t x, int8_t y, uint8_t coding);

static unsigned int _cost_lut[256][2];
static int          _initialized = 0;

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
		_cost_lut[0][0] = 128;
		_cost_lut[0][1] = 128;

		for (i=1; i<128; i++) {
			_cost_lut[i][0] = _cost_lut[256-i][1] = i + 128;
			_cost_lut[i][1] = _cost_lut[256-i][0] = 128 - i;
		}

		_initialized = 1;
	}

	ret->_backend = v;

	/* Compute the transition matrix */
	compute_trans(v);

	return ret;
}

static void
viterbi_deinit(HardSource *src)
{
	Viterbi *v = src->_backend;

	free(v->mem);
	free(v->tmp);
	free(v);
	free(src);
}

int
viterbi_decode(HardSource *src, uint8_t *out, size_t len)
{
	int fwd_depth;
	int in_pos, points_in, bytes_out;
	int i, count;
	int8_t in[2*MEM_DEPTH];
	unsigned int mincost;
	int8_t x, y;
	Node *best, (*tmp)[N_STATES];
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

		/* Run the Viterbi algorithm forward */
		for (i=0, in_pos=0; i<(int)points_in; i++) {
			/* Get a symbol from the source */
			x = in[in_pos++];
			y = in[in_pos++];

			/* Update the costs to be in each state given the input we have */
			update_costs(self, x, y);

			/* Update the Viterbi decoder memory (swap tmp and mem) */
			tmp = self->mem;
			self->mem = self->tmp;
			self->tmp = tmp;

			self->cur_depth++;
		}

		if (points_in < fwd_depth) {
			/* We reached the end of the source: just flush the Viterbi memory
			 * instead of doing the whole algorithm */
			return viterbi_flush(src, out, len);
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
			i = write_bits(out, best->data, count);
			out += i;
			bytes_out += i;

			/* Realign data and normalize the costs */
			mincost = best->cost;
			for (i=0; i<N_STATES; i++) {
				self->mem[i]->cost -= mincost;
				memmove(self->mem[i]->data, self->mem[i]->data + count/8, BACKTRACK_DEPTH/8);
				memset(self->mem[i]->data + BACKTRACK_DEPTH/8, '\0', (self->cur_depth-BACKTRACK_DEPTH)/8);
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
	Node *best;
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
	i = write_bits(out, best->data, MIN(maxlen, (size_t)self->cur_depth));

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
/* Recompute the cost function for every state given the input symbol (x, y) */
static inline void
update_costs(const Viterbi *self, int8_t x, int8_t y)
{
	Node *end_state;
	int id;
	int candidate_1, candidate_2;
	int cost_1, cost_2;
	int local_cost[4];
	uint8_t input;


	/* Prefetch any possible cost we might need while updating the individual
	 * state costs*/
	local_cost[0] = cost(x, y, 0);
	local_cost[1] = cost(x, y, 1);
	local_cost[2] = cost(x, y, 2);
	local_cost[3] = cost(x, y, 3);

	for (id=0; id<N_STATES; id++) {
		end_state = self->tmp[id];

		/* Compute the input necessary to get to end_state */
		input = id >> (K-1);

		/* Compute the costs from the two possible ancestors */
		candidate_1 = ((id << 1) & (N_STATES - 1));
		candidate_2 = candidate_1+1;
		cost_1 = self->mem[candidate_1]->cost +
		         local_cost[self->outputs[candidate_1][input]];
		cost_2 = self->mem[candidate_2]->cost +
		         local_cost[self->outputs[candidate_2][input]];

		/* Update the output string and cost */
		if (cost_1 < cost_2) {
			memcpy(end_state->data, self->mem[candidate_1]->data, self->cur_depth/8+1);
			end_state->data[self->cur_depth/8] |= input << (7-self->cur_depth%8);
			end_state->cost = cost_1;
		} else if (cost_2 < MAX_COST) {
			memcpy(end_state->data, self->mem[candidate_2]->data, self->cur_depth/8+1);
			end_state->data[self->cur_depth/8] |= input << (7-self->cur_depth%8);
			end_state->cost = cost_2;
		} else {
			end_state->cost = MAX_COST;
		}
	}
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

			v->outputs[state][input] = output;
		}
	}
}

/* Cost function */
static inline unsigned int
cost(int8_t x, int8_t y, uint8_t coding)
{
	return _cost_lut[(uint8_t)x][(coding >> 1) & 0x01] +
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
write_bits(uint8_t *out, const uint8_t *bits, size_t count)
{
	assert(!(count & 0x7));
	count /= 8;

	memcpy(out, bits, count);
	return count;
}
/*}}}*/

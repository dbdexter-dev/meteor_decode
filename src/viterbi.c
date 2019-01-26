#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "file.h"
#include "utils.h"
#include "viterbi.h"

static void dump_memory(const Viterbi *v);
static void compute_trans(Viterbi *v);
static int  find_best(const Viterbi *self, int8_t x, int8_t y, Path *end_state);
static int  parity(uint8_t x);
static int  write_bits(const uint8_t *bits, size_t count, uint8_t *out);

static unsigned int cost(int8_t x, int8_t y, int coding);

void
viterbi_deinit(Viterbi *v)
{
	unsigned int i;

	for (i=0; i<N_STATES; i++) {
		free(v->mem[i]);
		free(v->tmp[i]);
	}
	free(v);
}

Viterbi*
viterbi_init()
{
	int i;
	Viterbi *ret;

	ret = safealloc(sizeof(*ret));
	ret->cur_depth = 0;

	for (i=0; i<N_STATES; i++) {
		ret->mem[i] = calloc(1, sizeof(*ret->mem[0]));
		ret->mem[i]->id = i;
		ret->mem[i]->cost = 0;
		ret->mem[i]->data[0] = (i >> 7) & 0x01;

		ret->tmp[i] = calloc(1, sizeof(*ret->tmp[0]));
		ret->tmp[i]->id = i;
		ret->tmp[i]->cost = 0;
		ret->tmp[i]->data[0] = (i >> 7) & 0x01;
	}

	/* Compute the transition matrix */
	compute_trans(ret);

	return ret;
}

int
viterbi_decode(Viterbi *self, const int8_t *in, size_t len, uint8_t *out)
{
	unsigned fwd_depth;
	int in_pos, bytes_out;
	int i, count, cur_state;
	unsigned int mincost;
	int8_t x, y;
	Path *best, *tmp;

	/* Len must be a multiple of 8 */
	assert(!(len & 0x07));
	in_pos = 0;
	bytes_out = 0;

	/* Repeat until $len bytes have been read */
	while ((int)len > in_pos) {
/*		printf("Current depth: %d, max depth: %d\n", self->cur_depth, MEM_DEPTH);*/
		fwd_depth = MIN((size_t)(MEM_DEPTH - self->cur_depth), len/2);

		/* Run the Viterbi algorithm forward */
		for (i=0; i<(int)fwd_depth; i++) {
			/* Get a symbol from the source */
			x = in[in_pos++];
			y = in[in_pos++];

			/* For each state, find the path of least resistance to it */
			for (cur_state = 0; cur_state < N_STATES; cur_state++) {
				find_best(self, x, y, self->tmp[cur_state]);
			}

			/* Update the Viterbi decoder memory */
			for (cur_state=0; cur_state<N_STATES; cur_state++) {
				tmp = self->mem[cur_state];
				self->mem[cur_state] = self->tmp[cur_state];
				self->tmp[cur_state] = tmp;
			}
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
			i = write_bits(best->data, count, out);
			out += i;
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

/* Not really a part of the Viterbi algorithm, but still fits within this file.
 * Byte-wise convolutional encoder, same parameters as the decoder */
int
viterbi_encode(const uint8_t *in, size_t len, uint8_t *out)
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
static void
dump_memory(const Viterbi *self) 
{
	int i;

	printf("===============================\n");
	for (i=0; i<N_STATES; i++) {
		printf("%p ", (void*)self->mem[i]);
		printf("%p \t", (void*)self->tmp[i]);
	}
	printf("\n===============================\n");
	return;
}

/* Given an end state, find the previous state that gets to it with the least
 * effort */
static int
find_best(const Viterbi *self, int8_t x, int8_t y, Path *end_state)
{
	int start_state, prev;
	uint8_t input;
	unsigned int tmpcost, mincost;

	mincost = (unsigned int) -1;
	
	/* Compute the input necessary to get to end_state */
	input = end_state->id >> (K-1);


	/* Try with candidate #1 */
	start_state = ((end_state->id << 1) & (N_STATES - 1)) | 0x00;
	tmpcost = cost(x, y, self->trans[start_state][input].output);
	if (self->mem[start_state]->cost + tmpcost < MAX_COST) {
		mincost = self->mem[start_state]->cost + tmpcost;
		prev = start_state;
	}

	/* Try with candidate #2 */
	start_state = ((end_state->id << 1) & (N_STATES - 1)) | 0x01;
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
	const uint8_t mag = 128;
	unsigned int error;

	error = abs(x - (coding & 0x02 ? mag : -mag)) +
			abs(y - (coding & 0x01 ? mag : -mag));

	return error;
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

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <assert.h>
#include <string.h>
#include "protocol/cadu.h"
#include "utils.h"
#include "viterbi.h"

#define NEXT_DEPTH(x) (((x) + 1) % LEN(_vit.prev))
#define PREV_DEPTH(x) (((x) - 1 + LEN(_vit.prev)) % LEN(_vit.prev))
#define BETTER_METRIC(x, y) ((x) > (y))    /* Higher metric is better */
#define POLY_TOP_BITS ((G1 >> (K-1) & 1) << 1 | (G2 >> (K-1)))
#define TWIN_METRIC(metric, x, y) (\
	POLY_TOP_BITS == 0x0 ? (metric) : \
	POLY_TOP_BITS == 0x1 ? (metric)-2*(x) : \
	POLY_TOP_BITS == 0x2 ? (metric)-2*(y) : (-metric))

typedef struct {
	int16_t *restrict metric, *restrict next_metric;
	uint8_t prev[MEM_DEPTH][NUM_STATES/2];  /* State pairs always share their predecessors */
} Viterbi;

static int  parity(uint32_t word);
static int  metric(int x, int y, int coding);
static void update_metrics(int8_t x, int8_t y, int depth);
static void backtrace(uint8_t *out, uint8_t state, int depth, int bitskip, int bitcount);

static uint8_t _output_lut[NUM_STATES];
static int16_t _metric[NUM_STATES];
static int16_t _next_metric[NUM_STATES];
static Viterbi _vit;
static int _depth;

uint32_t
conv_encode_u32(uint64_t *output, uint32_t state, uint32_t data)
{
	int i;
	uint8_t tmp;

	*output = 0;
	for (i=31; i>=0; i--) {
		/* Compute new state */
		state = ((state >> 1) | ((data >> i) << (K-1))) & (NUM_STATES - 1);

		/* Compute output */
		tmp = parity(state & G1) << 1 | parity(state & G2);
		*output |= (uint64_t)tmp << (i<<1);
	}

	return state;
}

void
viterbi_init()
{
	int i, input, state, next_state, output;

	/* Precompute the output given a state and an input */
	for (state=0; state<NUM_STATES; state++) {
		input = 0;      /* Output for input=1 is _output_lut[state] ^ POLY_TOP_BITS */
		next_state = (state >> 1) | (input << (K-1));
		output = parity(next_state & G1) << 1 | parity(next_state & G2);

		_output_lut[state] = output;
	}

	/* Initialize current Viterbi depth */
	_depth = 0;

	/* Initialize state metrics in the backtrack memory */
	for (i=0; i<(int)LEN(_metric); i++) {
		_metric[i] = 0;
	}

	/* Bind metric arrays to the Viterbi struct */
	_vit.metric = _metric;
	_vit.next_metric = _next_metric;
}


int
viterbi_decode(uint8_t *restrict out, int8_t *restrict soft_cadu, int bytecount)
{
	int i;
	int best_metric;
	int total_metric;
	uint8_t best_state;
	int8_t x, y;

	assert(!(bytecount % (MEM_BACKTRACE>>3)));

	total_metric = 0;
	for(; bytecount > 0; bytecount -= MEM_BACKTRACE >> 3) {
		/* Viterbi forward step */
		for (i=MEM_START; i<(int)MEM_DEPTH; i++) {
			_depth = NEXT_DEPTH(_depth);

			y = *soft_cadu++;
			x = *soft_cadu++;

			update_metrics(-x, -y, _depth);
		}

		/* Find the state with the best metric */
		best_state = 0;
		best_metric = _vit.metric[0];
		for (i=1; i<(int)LEN(_metric); i++) {
			if (BETTER_METRIC(_vit.metric[i], best_metric)) {
				best_metric = _vit.metric[i];
				best_state = i;
			}
		}

		/* Resize metrics to prevent overflows */
		for (i=0; i<(int)LEN(_metric); i++) {
			_vit.metric[i] -= best_metric;
		}

		/* Update total metric */
		total_metric += (255 * MEM_BACKTRACE) - best_metric;

		/* Backtrace from the best state and write bits */
		backtrace(out, best_state, _depth, MEM_START, MEM_BACKTRACE);
		out += MEM_BACKTRACE >> 3;
	}

	return total_metric;
}


/* Static functions {{{ */
static int
parity(uint32_t word)
{
	/* From bit twiddling hacks */
	word ^= (word >> 1);
	word ^= (word >> 2);
	word = (word & 0x11111111) * 0x11111111;
	return (word >> 28) & 0x1;
}

static void
update_metrics(int8_t x, int8_t y, int depth)
{
	const int local_metrics[4] = {metric(x, y, 0), metric(x, y, 1),
	                              metric(x, y, 2), metric(x, y, 3)};
	uint8_t state, ns0, ns1, ns2, ns3, prev01, prev23;
	int16_t *const metric = _vit.metric;
	int16_t *const next_metric = _vit.next_metric;
	uint8_t *const prev_state = _vit.prev[depth];

#ifdef __ARM_NEON
	uint8_t start_states[] = {0, 2, 4, 6, 8, 10, 12, 14};
	int16_t lms[8];

	int16x8x2_t deint;
	int16x8_t best, next_metrics_vec;
	int16x8_t step_dup;

	uint16x8_t rev_compare;
	uint8x8_t states, prev;

	/* Load initial states */
	states = vld1_u8(start_states);

	for (state=0; state<NUM_STATES/2; state+=8) {
		/* Load metrics for 8 states and their twins, and compute the best ones */
		deint = vld2q_s16(&metric[state << 1]);
		best = BETTER_METRIC(1, 0) != 0
			? vmaxq_s16(deint.val[0], deint.val[1])
			: vminq_s16(deint.val[0], deint.val[1]);
		rev_compare = BETTER_METRIC(1, 0) != 0
			? vcltq_s16(deint.val[0], deint.val[1])
			: vcgtq_s16(deint.val[0], deint.val[1]);

		/* Collect previous states based on which ones had the best metric:
		 * start_state[i] if best was in [0], else start_state[i] + 1 */
		prev = vadd_u8(states, vand_u8(vmov_n_u8(1), vqmovn_u16(rev_compare)));
		vst1_u8(&prev_state[state], prev);

		/* Compute local path metrics */
		lms[0] = local_metrics[_output_lut[(state<<1)]];      /* lm0/0 */
		lms[1] = TWIN_METRIC(lms[0], x, y);                 /* lm2/0 */
		lms[2] = local_metrics[_output_lut[(state<<1)+4]];    /* lm0/+2 */
		lms[3] = TWIN_METRIC(lms[2], x, y);                 /* lm2 */
		lms[4] = local_metrics[_output_lut[(state<<1)+8]];    /* lm0 */
		lms[5] = TWIN_METRIC(lms[4], x, y);                 /* lm2 */
		lms[6] = local_metrics[_output_lut[(state<<1)+12]];    /* lm0 */
		lms[7] = TWIN_METRIC(lms[6], x, y);                 /* lm2 */

		/* Updathe path metrics */
		next_metrics_vec = vld1q_s16(lms);           /* Load #1: lm0, lm2, ... */
		next_metrics_vec = vaddq_s16(next_metrics_vec, best);
		vst1q_s16(&next_metric[state], next_metrics_vec);

		lms[0] = lms[1];
		lms[1] = TWIN_METRIC(lms[0], x, y);
		lms[2] = lms[3];
		lms[3] = TWIN_METRIC(lms[2], x, y);
		lms[4] = lms[5];
		lms[5] = TWIN_METRIC(lms[4], x, y);
		lms[6] = lms[7];
		lms[7] = TWIN_METRIC(lms[6], x, y);

		next_metrics_vec = vld1q_s16(lms);           /* Load #2: lm1, lm3, ... */
		next_metrics_vec = vaddq_s16(next_metrics_vec, best);
		vst1q_s16(&next_metric[state + (1<<(K-1))], next_metrics_vec);

		/* Go to the next state set */
		states = vadd_u8(states, vmov_n_u8(2*LEN(start_states)));
	}
#else
	int16_t metric0, metric1, metric2, metric3, best01, best23;
	int16_t lm0, lm1, lm2, lm3;

	for (state=0; state<NUM_STATES/2; state+=2) {
		/* ns2 and ns3 are very closely related to ns0 and ns1: they have the
		 * same local metrics as ns1 and ns0 respectively. Computing them here
		 * reduces memory accesses, and improves cache locality. */

		/* Compute the two possible next states */
		ns0 = state;
		ns1 = state + (1 << (K-1));
		ns2 = ns0 + 1;
		ns3 = ns1 + 1;

		/* Fetch the metrics of the two possible predecessors */
		metric0 = metric[state<<1];
		metric1 = metric[(state<<1)+1];
		metric2 = metric[(state<<1)+2];
		metric3 = metric[(state<<1)+3];


		/* Select the state that has the best metric between the two */
		best01 = BETTER_METRIC(metric0, metric1) ? metric0 : metric1;
		prev01 = BETTER_METRIC(metric0, metric1) ? (state<<1) : (state<<1) + 1;
		best23 = BETTER_METRIC(metric2, metric3) ? metric2 : metric3;
		prev23 = BETTER_METRIC(metric2, metric3) ? (state<<1) + 2 : (state<<1) + 3;

		/* ns0 and ns1 have the same ancestor, just different metrics. Save it
		 * only once for both. Same applies for ns2 and ns3 */
		prev_state[ns0] = prev01;
		prev_state[ns2] = prev23;

		/* Compute the metrics of the ns0/ns1 transitions */
		lm0 = local_metrics[_output_lut[state<<1]]; /* metric to ns0/1 given in=0 */
		lm1 = TWIN_METRIC(lm0, x, y);               /* metric to ns0/1 given in=1 */
		lm2 = lm1;                                  /* metric to ns2/3 given in=0 */
		lm3 = TWIN_METRIC(lm2, x, y);               /* metric to ns2/3 given in=1 */

		/* Metric of the next state = best predecessor metric + local metric */
		next_metric[ns0] = best01 + lm0;
		next_metric[ns1] = best01 + lm1;
		next_metric[ns2] = best23 + lm2;
		next_metric[ns3] = best23 + lm3;
	}
#endif

	/* Swap metric and next_metric for the next iteration */
	_vit.metric = next_metric;
	_vit.next_metric = metric;
}

static void
backtrace(uint8_t *out, uint8_t state, int depth, int bitskip, int bitcount)
{
	int i, bytecount;
	uint8_t tmp;

	assert(!(bitcount & 0x7));

	/* Backtrace without writing bits */
	for (; bitskip > 0; bitskip--) {
		state = _vit.prev[depth][state & ~(1<<(K-1))];
		depth = PREV_DEPTH(depth);
	}

	/* Preemptively advance out: bits are written in reverse order */
	bytecount = bitcount >> 3;
	out += bytecount;

	/* Backtrace while writing bits */
	for (;bytecount > 0; bytecount--) {
		tmp = 0;
		/* Process each byte separately */
		for (i=0; i<8; i++) {
			tmp |= (state >> (K-1)) << i;
			state = _vit.prev[depth][state & ~(1<<(K-1))];
			depth = PREV_DEPTH(depth);
		}

		/* Copy byte to output, then go to the previous byte */
		*--out = tmp;
	}
}

static inline int
metric(int x, int y, int coding)
{
	return ((coding >> 1) ? x : -x) +
	       ((coding &  1) ? y : -y);
}
/* }}} */

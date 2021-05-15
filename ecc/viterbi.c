#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <assert.h>
#include <string.h>
#ifdef __ARM_FEATURE_SIMD32
#include "math/arm_simd32.h"
#endif
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
	int16_t *restrict metrics, *restrict next_metrics;
	uint8_t prev[MEM_DEPTH][NUM_STATES/2];  /* State pairs always share their predecessors */
} Viterbi;

static int  parity(uint32_t word);
static int  metric(int x, int y, int coding);
static void update_metrics(int8_t x, int8_t y, int depth);
static void backtrace(uint8_t *out, uint8_t state, int depth, int bitskip, int bitcount);

static uint8_t _output_lut[NUM_STATES];
static int16_t _metric[NUM_STATES];
static int16_t _next_metrics[NUM_STATES];
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

#if defined(__ARM_FEATURE_SIMD32) && !defined(__ARM_NEON)
		_output_lut[state] = output << 3;
#else
		_output_lut[state] = output;
#endif
	}

	/* Initialize current Viterbi depth */
	_depth = 0;

	/* Initialize state metrics in the backtrack memory */
	for (i=0; i<(int)LEN(_metric); i++) {
		_metric[i] = 0;
	}

	/* Bind metric arrays to the Viterbi struct */
	_vit.metrics = _metric;
	_vit.next_metrics = _next_metrics;
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
		best_metric = _vit.metrics[0];
		for (i=1; i<(int)LEN(_metric); i++) {
			if (BETTER_METRIC(_vit.metrics[i], best_metric)) {
				best_metric = _vit.metrics[i];
				best_state = i;
			}
		}

		/* Resize metrics to prevent overflows */
		for (i=0; i<(int)LEN(_metric); i++) {
			_vit.metrics[i] -= best_metric;
		}

		/* Update total metric */
		total_metric += 2 * ((127 * MEM_BACKTRACE) - best_metric);

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
	int16_t *const metrics = _vit.metrics;
	int16_t *const next_metrics = _vit.next_metrics;
	uint8_t *const prev_state = _vit.prev[depth];
	uint8_t state;

#if defined(__ARM_NEON) && (POLY_TOP_BITS == 0x3 || POLY_TOP_BITS == 0x0)
	const int8_t local_metrics[4] = {metric(x, y, 0), metric(x, y, 1),
	                                  metric(x, y, 2), metric(x, y, 3)};
	uint8_t start_states[] = {0, 2, 4, 6, 8, 10, 12, 14};

	int16x8x2_t deint;
	int16x8_t best, next_metrics_vec;
	int16x8_t step_dup;

	uint16x8_t rev_compare;
	uint8x8_t states, prev;
	int8x8_t cost_vec;
	int16x8_t metrics_vec;

	/* Load initial states and local metrics. States is just a collection of all
	 * the even states currently being considered */
	states = vld1_u8(start_states);
	const int8x8_t local_metrics_lut = vld1_s8(local_metrics);

	for (state=0; state<NUM_STATES/2; state+=8) {
		/* Load metrics for 8 states and their twins, and compute the best ones */
		deint = vld2q_s16(&metrics[state << 1]);
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

		/* Get the local metrics based on the output LUT. */
		cost_vec = vld1_s8(&_output_lut[state<<1]);
		metrics_vec = vmovl_s8(vtbl1_s8(local_metrics_lut, cost_vec));
		metrics_vec = vuzpq_s16(metrics_vec, metrics_vec).val[0];

		/* Updathe path metrics */
		next_metrics_vec = metrics_vec;           /* Load #1: lm0, lm2, ... */
		next_metrics_vec = vaddq_s16(next_metrics_vec, best);
		vst1q_s16(&next_metrics[state], next_metrics_vec);

		/* Derive new metrics from old metrics. TODO implement for other G1,G2 values*/
		metrics_vec = POLY_TOP_BITS == 0x00 ? metrics_vec
		            : POLY_TOP_BITS == 0x03 ? vnegq_s16(metrics_vec)
		            : metrics_vec;

		next_metrics_vec = metrics_vec;           /* Load #2: lm1, lm3, ... */
		next_metrics_vec = vaddq_s16(next_metrics_vec, best);
		vst1q_s16(&next_metrics[state + (1<<(K-1))], next_metrics_vec);

		/* Go to the next state set */
		states = vadd_u8(states, vmov_n_u8(2*LEN(start_states)));
	}
#elif __ARM_FEATURE_SIMD32 == 1
	const uint32_t local_metrics = (uint8_t)metric(x, y, 0)
	                             | ((uint8_t)metric(x, y, 1) << 8)
	                             | ((uint8_t)metric(x, y, 2) << 16)
	                             | ((uint8_t)metric(x, y, 3) << 24);

	int16_t lm0, lm1, lm2, lm3;
	uint8_t ns0, ns1, ns2, ns3;

	uint32_t metric02, metric13;
	uint32_t metric_tmp;
	uint32_t best01_23, prev01_23, lms;
	uint32_t state_vec;

	for (state=0; state<NUM_STATES/2; state+=2) {
		/* Compute the two possible next states */
		ns0 = state;
		ns1 = state + (1 << (K-1));
		ns2 = ns0 + 1;
		ns3 = ns1 + 1;

		state_vec = (state<<1) << 16 | (state<<1) + 2;

		/* Fetch the metrics of the two possible predecessors and their twins */
		metric02 = *(uint32_t*)&metrics[state<<1];
		metric13 = *(uint32_t*)&metrics[(state<<1)+2];

		/* Combine them to prepare for some SIMD magic */
		metric_tmp = __pkhbt(metric13, metric02, 16);
		metric13 = __pkhtb(metric02, metric13, 16);
		metric02 = metric_tmp;

		/* Compute best metric and prev states */
		__ssub16(metric02, metric13);
		best01_23 = __sel(metric02, metric13);
		prev01_23 = __ssub16(state_vec, __sel(0, ~0));  /* ~0 is immediate encodable, 0x00010001 is not */

		prev_state[ns0] = prev01_23 & 0xFF;
		prev_state[ns2] = prev01_23 >> 16;

		/* Compute the metrics of the ns0/ns1/ns2/ns3 transitions */
		lm0 = (int8_t)((local_metrics >> _output_lut[state<<1]) & 0xFF);
		lm1 = TWIN_METRIC(lm0, x, y);               /* metric to ns0/1 given in=1 */
		lm2 = lm1;                                  /* metric to ns2/3 given in=0 */
		lm3 = TWIN_METRIC(lm2, x, y);               /* metric to ns2/3 given in=1 */

		/* Save new metrics */
		lms = (lm0<<16) + lm2;
		*(uint32_t*)&next_metrics[ns0] = __sadd16(best01_23, lms);
		lms = POLY_TOP_BITS == 0x00 ? lms
		    : POLY_TOP_BITS == 0x03 ? (uint32_t)__ssub16(0, (uint32_t)lms)
		    : (uint32_t)((lm1<<16) + lm3);
		*(uint32_t*)&next_metrics[ns1] = __sadd16(best01_23, lms);
	}
#else

	#if defined(__ARM_NEON) && POLY_TOP_BITS != 0x3 && POLY_TOP_BITS != 0x0
	#warn "NEON acceleration unimplemented for the given G1/G2, using default implementation"
	#endif
	const int local_metrics[4] = {metric(x, y, 0), metric(x, y, 1),
	                              metric(x, y, 2), metric(x, y, 3)};
	int16_t metric0, metric1, metric2, metric3, best01, best23;
	int16_t lm0, lm1, lm2, lm3;
	uint8_t ns0, ns1, ns2, ns3, prev01, prev23;

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
		metric0 = metrics[state<<1];
		metric1 = metrics[(state<<1)+1];
		metric2 = metrics[(state<<1)+2];
		metric3 = metrics[(state<<1)+3];


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
		next_metrics[ns0] = best01 + lm0;
		next_metrics[ns1] = best01 + lm1;
		next_metrics[ns2] = best23 + lm2;
		next_metrics[ns3] = best23 + lm3;
	}
#endif

	/* Swap metric and next_metrics for the next iteration */
	_vit.metrics = next_metrics;
	_vit.next_metrics = metrics;
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
	/* NOTE: metric is shifted down by 1 so that it can fit inside an int8_t,
	 * which makes NEON acceleration so much better. The results are exactly the
	 * same as far as I can tell, even though we're losing 1 bit of precision on
	 * the metric, and the upside is that this makes the decoding ~10% faster */
	return MAX(-128, MIN(127, (((coding >> 1) ? x : -x) +
	       ((coding &  1) ? y : -y))>>1));
}
/* }}} */

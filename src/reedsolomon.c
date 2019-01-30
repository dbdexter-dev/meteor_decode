#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packet.h"
#include "reedsolomon.h"
#include "utils.h"

static int  fix_block(ReedSolomon *self, uint8_t *data, size_t len);
static void deinterleave(uint8_t *out, const uint8_t *in, int step, int delta, size_t len);
static void interleave(uint8_t *out, const uint8_t *in, int step, int delta, size_t len);

static uint8_t  gf_mul(uint8_t x, uint8_t y);
static uint8_t  gf_div(uint8_t x, uint8_t y);
static uint8_t  gf_inv(uint8_t x);
static uint8_t  gf_pow(uint8_t x, int exp);
static uint8_t  gf_poly_eval(const uint8_t *poly, uint8_t x, size_t len);
static uint8_t* gf_poly_mul(const uint8_t *x, const uint8_t *y, size_t len_x, size_t len_y);
static uint8_t* gf_poly_deriv(const uint8_t *x, size_t len);

static uint8_t _alpha[RS_N];
static uint8_t _logtable[RS_N];

ReedSolomon*
rs_init(size_t size, int interleaving)
{
	int i, exp;
	uint16_t tmp;
	ReedSolomon* ret;

	ret = safealloc(sizeof(*ret));
	ret->interleaving = interleaving;
	ret->block = safealloc(size / interleaving);

	/* Compute the log-multiplication matrix */
	_alpha[0] = 0x01;
	_logtable[0x01] = 0;
	for (i=1; i<RS_N; i++) {
		/* next_alpha = (alpha << 1) MOD GEN_POLY */
		tmp = _alpha[i-1] << 1;
		tmp = (tmp & 0x100 ? tmp ^ GEN_POLY : tmp);
		_alpha[i] = tmp;
		_logtable[tmp] = (uint8_t)i;
	}

	/* Compute the code generator polynomial */
	for (i=0; i<RS_2T; i++) {
		exp = ((FIRST_CONSEC_ROOT + i) * 11) % RS_N;
		ret->poly_zeroes[i] = _alpha[exp];
	}

	return ret;
}

void
rs_deinit(ReedSolomon *r)
{
	free(r->block);
	free(r);
}

/* Check https://public.ccsds.org/Pubs/101x0b4s.pdf to learn more about the RS
 * code in use by the LRPT standard */
int
rs_fix_packet(ReedSolomon *self, Cvcdu *pkt, int *fixes)
{
	int i;
	int ret;
	int corr_count;
	size_t block_size;

	block_size = sizeof(*pkt)/self->interleaving;
	ret = 0;
	for (i=0; i<self->interleaving; i++) {
		/* Separate an interleaved block */
		deinterleave(self->block, (uint8_t*)pkt, self->interleaving, i, block_size);

		/* Apply RS error correction */
		corr_count = fix_block(self, self->block, RS_N);
		if (fixes) {
			fixes[i] = corr_count;
		}

		ret = (ret>=0 && corr_count >= 0) ? ret+corr_count : -1;

		/* Re-interleave the (hopefully) corrected packet */
		interleave((uint8_t*)pkt, self->block, self->interleaving, i, sizeof(pkt));
	}

	return ret;
}


/* Static functions {{{*/
static int
fix_block(ReedSolomon *self, uint8_t *block, size_t len)
{
	int i, m, n;
	int to_correct;
	uint8_t syndrome[RS_2T];
	uint8_t *omega, *lambda_prime;
	uint8_t lambda[RS_T+1], prev_lambda[RS_T+1], tmp[RS_T+1];
	uint8_t delta, prev_delta;
	int lambda_deg;
	uint8_t lambda_root[RS_T], error_pos[RS_T];
	int error_count;
	uint8_t num, den, fcr;

	/* Compute syndromes */
	to_correct = 0;
	for (i=0; i<RS_2T; i++) {
		syndrome[i] = gf_poly_eval(block, self->poly_zeroes[i], len);
		to_correct |= syndrome[i];
	}
	if (!to_correct) {
		return 0;
	}

	/* Compute lambda using Berlekamp-Massey */
	memset(lambda, 0, sizeof(lambda));
	memset(prev_lambda, 0, sizeof(prev_lambda));
	lambda[0] = prev_lambda[0] = 1;
	lambda_deg = 0;
	prev_delta = 1;
	m = 1;
	for (n=0; n<RS_2T; n++) {
		/* Discrepancy = sum_i=1^lambda_deg{syndrome[n-i] * lambda[i]} */
		for (i=0, delta=0; i<=lambda_deg; i++) {
			delta ^= gf_mul(syndrome[n-i], lambda[i]);
		}

		if (delta == 0) {
			m++;
		} else if (n >= 2*lambda_deg) {
			for (i=0; i<RS_T+1; i++) {
				tmp[i] = lambda[i];
			}
			for (i=m; i<RS_T+1; i++) {
				lambda[i] ^= gf_mul(gf_div(delta, prev_delta), prev_lambda[i-m]);
			}
			for (i=0; i<RS_T+1; i++) {
				prev_lambda[i] = tmp[i];
			}

			prev_delta = delta;
			lambda_deg = n + 1 - lambda_deg;
			m = 1;
		} else {
			for (i=m; i<RS_T+1; i++) {
				lambda[i] ^= gf_mul(gf_div(delta, prev_delta), prev_lambda[i-m]);
			}
			m++;
		}
	}

	/* Bruteforce the roots of lambda */
	for (i=0, error_count=0; i<RS_N; i++) {
		if (gf_poly_eval(lambda, i, RS_T+1) == 0) {
			/* This is black magic */
			error_pos[error_count] = RS_N - (_logtable[i] * (DUAL_BASIS_BASE-1)) % RS_N;
			lambda_root[error_count] = i;
			error_count++;
			if (error_count > RS_T) {
				return -1;
			}
		}
	}
	if (error_count != lambda_deg) {
		return -1;
	}

	/* Compute omega and lambda_prime, necessary for the Forney algorithm */
	omega = gf_poly_mul(syndrome, lambda, RS_2T, RS_T+1);
	lambda_prime = gf_poly_deriv(lambda, RS_T+1);

	/* Fix errors in the block */
	for (i=0; i<error_count; i++) {
		/* lambda_root = 1/Xi, Xi being the i-th error locator */
		fcr = gf_pow(lambda_root[i], FIRST_CONSEC_ROOT-1);
		num = gf_poly_eval(omega,       lambda_root[i], RS_2T);
		den = gf_poly_eval(lambda_prime,lambda_root[i], RS_T);

		if (error_pos[i] < len) {
			block[error_pos[i]] ^= gf_div(gf_mul(num, fcr), den);
		}
	}

	free(omega);
	free(lambda_prime);
	return error_count;
}

static void
deinterleave(uint8_t *out, const uint8_t *in, int step, int delta, size_t len)
{
	size_t i;
	for (i=0; i<len; i++) {
		out[i] = in[i*step + delta];
	}
}

static void 
interleave(uint8_t *out, const uint8_t *in, int step, int delta, size_t len)
{
	size_t i;
	for (i=0; i<len; i++) {
		out[i*step + delta] = in[i];
	}
}
/* Galois field static functions {{{*/
/* Evaluate a message polynomial (degree = N) at a point x */
static uint8_t
gf_poly_eval(const uint8_t *poly, uint8_t x, size_t poly_size)
{
	int i;
	uint8_t ret;

	ret = 0;
	for (i=poly_size-1; i>=0; i--) {
		ret = gf_mul(ret, x) ^ poly[i];
	}

	return ret;
}

/* Polynomial multiplication modulo len_x */
static uint8_t*
gf_poly_mul(const uint8_t *x, const uint8_t *y, size_t len_x, size_t len_y)
{
	size_t i, j;
	uint8_t *ret;

	ret = calloc(len_x, sizeof(*ret));

	for (j=0; j<len_y; j++) {
		for (i=0; i<len_x; i++) {
			if (i+j < len_x) {
				ret[i+j] ^= gf_mul(x[i], y[j]);
			}
		}
	}

	return ret;
}

/* Formal derivative */
static uint8_t*
gf_poly_deriv(const uint8_t *x, size_t len)
{
	size_t i, j;
	uint8_t *ret;

	ret = calloc(len-1, sizeof(*ret));
	for (i=1; i<len; i++) {
		for (j=0; j<i; j++) {
			ret[i-1] ^= x[i];
		}
	}

	return ret;
}


/* Multiply two GF elements */
static uint8_t
gf_mul(uint8_t x, uint8_t y)
{
	if (x==0 || y==0) {
		return 0;
	}
	return _alpha[(_logtable[x] + _logtable[y]) % RS_N];
}

static uint8_t
gf_div(uint8_t x, uint8_t y)
{
	if (x==0) {
		return 0;
	}
	if (y==0) {
		fatal("DIV0!");
	}
	return _alpha[(_logtable[x] - _logtable[y] + RS_N) % RS_N];
}
static uint8_t
gf_inv(uint8_t x)
{
	return _alpha[RS_N - _logtable[x]];
}
static uint8_t
gf_pow(uint8_t x, int exp)
{
	return _alpha[(_logtable[x] * exp) % RS_N];
}

/*}}}*/
/*}}}*/

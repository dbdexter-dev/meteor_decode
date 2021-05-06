#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "protocol/vcdu.h"
#include "rs.h"
#include "utils.h"

static int fix_block(uint8_t *data);

static uint8_t gfmul(uint8_t x, uint8_t y);
static uint8_t gfdiv(uint8_t x, uint8_t y);
static uint8_t gfpow(uint8_t x, int exp);
static void poly_deriv(uint8_t *dst, const uint8_t *poly, int len);
static uint8_t poly_eval(const uint8_t *poly, uint8_t x, int len);
static void poly_mul(uint8_t *dst, const uint8_t *poly1, const uint8_t *poly2, int len_1, int len_2);

static uint8_t _alpha[RS_N+1];
static uint8_t _logtable[RS_N+1];
static uint8_t _gaproot[RS_N];
static uint8_t _zeroes[RS_T];

void
rs_init()
{
	int i, tmp;
	int exp;

	_alpha[0] = 1;
	_logtable[1] = 0;
     /* Not entirely sure what's going on here, but the logtable doesn't have
      * this number anywhere, and setting log[0] = 183 fixes the _gaproot error */
	_logtable[0] = 183;

	/* Initialize the exponent and logarithm tables */
	for (i=1; i<RS_N; i++) {
		tmp = (int)_alpha[i-1] << 1;
		tmp = (tmp > RS_N ? tmp ^ GEN_POLY : tmp);
		_alpha[i] = tmp;
		_logtable[tmp] = i;
	}

	_logtable[255] = 183;
	_alpha[255] = 0;

	/* Compute the gap'th log */
	for (i=0; i<RS_N; i++) {
		_gaproot[gfpow(i, ROOT_SKIP)] = i;
	}

	/* Compute the roots of the generator polynomial */
	for (i=0; i<RS_T; i++) {
		exp = ((i + FIRST_ROOT) * ROOT_SKIP) % RS_N;
		_zeroes[i] = _alpha[exp];
	}
}

int
rs_fix(Vcdu *c)
{
	int i, j;
	int errors, errdelta;
	uint8_t block[RS_N];
	uint8_t *const data_start = (uint8_t*)c;

	errors = 0;
	for (i=0; i<INTERLEAVING; i++) {
		/* Deinterleave */
		for (j=0; j<RS_N; j++) {
			block[j] = data_start[j*INTERLEAVING + i];
		}

		/* Fix errors */
		errdelta = fix_block(block);
		if (errdelta < 0 || errors < 0) {
			errors = -1;
		} else {
			errors += errdelta;
		}

		/* Reinterleave */
		for (j=0; j<RS_N; j++) {
			data_start[j*INTERLEAVING + i] = block[j];
		}
	}

	return errors;
}

/* Static functions {{{ */
static int
fix_block(uint8_t *data)
{
	int i, m, n, delta, prev_delta;
	int lambda_deg;
	int has_errors;
	int error_count;
	uint8_t syndrome[RS_T];
	uint8_t lambda[RS_T2+1], prev_lambda[RS_T2+1], tmp[RS_T2+1];
	uint8_t lambda_root[RS_T2], error_pos[RS_T2];
	uint8_t omega[RS_T], lambda_prime[RS_T2];
	uint8_t num, den, fcr;

	/* Compute syndromes */
	has_errors = 0;
	for (i=0; i<RS_T; i++) {
		syndrome[i] = poly_eval(data, _zeroes[i], RS_N);
		has_errors |= syndrome[i];
	}
	if (!has_errors) {
		return 0;
	}


	/* Berlekamp-Massey algorithm */
	memset(lambda, 0, sizeof(lambda));
	memset(prev_lambda, 0, sizeof(prev_lambda));
	lambda_deg = 0;
	prev_delta = 1;
	lambda[0] = prev_lambda[0] = 1;
	m = 1;

	for (n=0; n<RS_T; n++) {
		delta = syndrome[n];
		for (i=1; i<=lambda_deg; i++) {
			delta ^= gfmul(syndrome[n-i], lambda[i]);
		}

		if (delta == 0) {
			m++;
		} else if (2*lambda_deg <= n) {
			for (i=0; i<RS_T2+1; i++) {
				tmp[i] = lambda[i];
			}
			for (i=m; i<RS_T2+1; i++) {
				lambda[i] ^= gfmul(gfdiv(delta, prev_delta), prev_lambda[i-m]);
			}
			for (i=0; i<RS_T2+1; i++) {
				prev_lambda[i] = tmp[i];
			}

			prev_delta = delta;
			lambda_deg = n + 1 - lambda_deg;
			m = 1;
		} else {
			for (i=m; i<RS_T2+1; i++) {
				lambda[i] ^= gfmul(gfdiv(delta, prev_delta), prev_lambda[i-m]);
			}
			m++;
		}
	}

	/* Roots bruteforcing */
	error_count = 0;
	for (i=1; i<=RS_N && error_count < lambda_deg; i++) {
		if (poly_eval(lambda, i, RS_T2+1) == 0) {
			lambda_root[error_count] = i;
			error_pos[error_count] = _logtable[_gaproot[gfdiv(1, i)]];
			error_count++;
		}
	}

	if (error_count != lambda_deg) {
		return -1;
	}

	poly_mul(omega, syndrome, lambda, RS_T, RS_T2+1);
	poly_deriv(lambda_prime, lambda, RS_T2+1);

	/* Fix errors in the block */
	for (i=0; i<error_count; i++) {
		/* lambda_root[i] = 1/Xi, Xi being the i-th error locator */
		fcr = gfpow(lambda_root[i], FIRST_ROOT-1);
		num = poly_eval(omega, lambda_root[i], RS_T);
		den = poly_eval(lambda_prime, lambda_root[i], RS_T2);

		data[error_pos[i]] ^= gfdiv(gfmul(num, fcr), den);
	}

	return error_count;
}

static uint8_t
poly_eval(const uint8_t *poly, uint8_t x, int len)
{
	uint8_t ret;

	ret = 0;
	for (len--; len>=0; len--) {
		ret = gfmul(ret, x) ^ poly[len];
	}

	return ret;
}

static void
poly_deriv(uint8_t *dst, const uint8_t *poly, int len)
{
	int i, j;

	for (i=1; i<len; i++) {
		dst[i-1] = 0;
		for (j=0; j<i; j++) {
			dst[i-1] ^= poly[i];
		}
	}
}

static void
poly_mul(uint8_t *dst, const uint8_t *poly1, const uint8_t *poly2, int len_1, int len_2)
{
	int i, j;

	for (i=0; i<len_1; i++) {
		dst[i] = 0;
	}

	for (j=0; j<len_2; j++) {
		for (i=0; i<len_1; i++) {
			if (i+j < len_1) {
				dst[i+j] ^= gfmul(poly1[i], poly2[j]);
			}
		}
	}
}

static uint8_t
gfmul(uint8_t x, uint8_t y)
{
	if (x==0 || y==0) {
		return 0;
	}

	return _alpha[(_logtable[x] + _logtable[y]) % RS_N];
}

static uint8_t
gfdiv(uint8_t x, uint8_t y)
{
	if (x == 0 || y == 0) {
		return 0;
	}

	return _alpha[(_logtable[x] - _logtable[y] + RS_N) % RS_N];
}

static uint8_t
gfpow(uint8_t x, int exp)
{
	return x == 0 ? 0 : _alpha[(_logtable[x] * exp) % RS_N];
}
/* }}} */

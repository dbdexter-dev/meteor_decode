#include <stdint.h>
#include <stdlib.h>
#include "diffcode.h"
#include "math/int.h"

static inline int8_t signsqrt(int x);
static int _prev_i, _prev_q;

void
diff_decode(int8_t *soft_cadu, size_t len)
{
	size_t i;
	int x, y, tmpi, tmpq;

	tmpq = soft_cadu[0];
	tmpi = soft_cadu[1];

	soft_cadu[0] = signsqrt(tmpq * _prev_q);
	soft_cadu[1] = signsqrt(-tmpi * _prev_i);

	for (i=2; i<len; i+=2) {
		x = soft_cadu[i];
		y = soft_cadu[i+1];

		soft_cadu[i] = signsqrt(soft_cadu[i] * tmpq);
		soft_cadu[i+1] = signsqrt(-soft_cadu[i+1] * tmpi);

		tmpq = x;
		tmpi = y;
	}

	_prev_i = tmpi;
	_prev_q = tmpq;
}

static inline int8_t
signsqrt(int x)
{
	return (x > 0) ? int_sqrt(x) : -int_sqrt(-x);
}

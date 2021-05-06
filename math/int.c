#include "int.h"
#include "utils.h"

unsigned int
int_sqrt(unsigned int x)
{
	int i;
	const int x2 = x >> 1;
	int guess;

	guess = 1 << ((32-__builtin_clz(x)) >> 1);
	if (guess < 2) {
		return 0;
	}

	PRAGMA_UNROLL(4)
	for (i=0; i<SQRT_PRECISION; i++) {
		guess = ((guess >> 1) + x2/guess);
	}

	return guess;
}

#include <math.h>
#include "source.h"
#include "utils.h"

static void diff_close(SoftSource *self);
static inline int signsqrt(int x);
static int diff_read(SoftSource *self, int8_t *buf, size_t count);

SoftSource*
diff_src(SoftSource *src)
{
	SoftSource *ret;

	ret = safealloc(sizeof(ret));
	ret->read = diff_read;
	ret->close = diff_close;
	ret->_backend = src;

	return ret;
}

static void
diff_close(SoftSource *self)
{
	free(self->_backend);
	free(self);
}

static int
diff_read(SoftSource *self, int8_t *buf, size_t count)
{
	int i;
	int x, y;
	int tmp1, tmp2;
	int samples_read;
	static int prev_i = 0;
	static int prev_q = 0;
	SoftSource *src = self->_backend;

	samples_read = src->read(src, buf, count);

	tmp1 = buf[0];
	tmp2 = buf[1];

	buf[0] = signsqrt(buf[0] * prev_i);
	buf[1] = signsqrt(-buf[1] * prev_q);

	for (i=2; i<=samples_read-2; i+=2) {
		x = buf[i];
		y = buf[i+1];

		buf[i] = signsqrt(buf[i] * tmp1);
		buf[i+1] = signsqrt(-buf[i+1] * tmp2);

		tmp1 = x;
		tmp2 = y;
	}


	prev_i = tmp1;
	prev_q = tmp2;
	return samples_read;
}

static inline int
signsqrt(int x)
{
	return x > 0 ? sqrt(x) : -sqrt(-x);
}

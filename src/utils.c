#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utils.h"


static void init();

/* 90 degree phase shift + phase mirroring lookup table
 * XXX HARD BYTES, NOT SOFT SAMPLES XXX
 */
static int8_t _rot_lut[256];
static int8_t _inv_lut[256];
static int8_t _ones[256];
static int    _initialized = 0;
static char _time_of_day[sizeof("HH:MM:SS.mmm")];

/* Expand hard 1-bit samples to 8 bits */
void
bit_expand(uint8_t *dst, const uint8_t *src, size_t len)
{
	int i, j;

	for (i=0; i<(int)len; i++) {
		for (j=0; j<8; j++) {
			dst[i*8+j] = (src[i] >> (7-j)) & 0x01;
		}
	}
}

/* Compute the correlation index between two buffers */
int
correlation(const uint8_t *x, const uint8_t *y, int len)
{
	int i, ret;

	ret = len * 8;
	for (i=0; i<len; i++) {
		ret -= count_ones(*x++ ^ *y++);
	}

	return ret;
}

/* Rotate a bit pattern 90 degrees in phase */
void
iq_rotate_hard(uint8_t *buf, size_t count, Phase p)
{
	size_t i;

	if (!_initialized) {
		init();
	}

	for (i=0; i<count; i++) {
		if (p == PHASE_90 || p == PHASE_270) {
			buf[i] = _rot_lut[buf[i]];
		}
		if (p == PHASE_180 || p == PHASE_270) {
			buf[i] ^= 0xFF;
		}
	}
}

/* Same as above but for soft bit patterns */
void
iq_rotate_soft(int8_t *buf, size_t count, Phase p)
{
	size_t i;
	int8_t tmp;

	if (!_initialized) {
		init();
	}

	switch(p) {
	case PHASE_90:
		for (i=0; i<count; i+=2) {
			tmp = buf[i];
			buf[i] = -buf[i+1];
			buf[i+1] = tmp;
		}
		break;
	case PHASE_180:
		for (i=0; i<count; i++) {
			buf[i] = -buf[i];
		}
		break;
	case PHASE_270:
		for (i=0; i<count; i+=2) {
			tmp = buf[i];
			buf[i] = buf[i+1];
			buf[i+1] = -tmp;
		}
		break;
	default:
		break;
	}
}

/* Flip I<->Q in a bit pattern */
void
iq_reverse_hard(uint8_t *buf, size_t count)
{
	size_t i;

	if (!_initialized) {
		init();
	}

	for (i=0; i<count; i++) {
		buf[i] = _inv_lut[buf[i]];
	}
}

/* Same as above but for soft bit patterns */
void
iq_reverse_soft(int8_t *buf, size_t count)
{
	size_t i;
	int8_t tmp;

	if (!_initialized) {
		init();
	}

	for (i=0; i<count; i+=2) {
		tmp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = tmp;
	}
}

/* Count the ones in a uint8_t */
int
count_ones(uint8_t val)
{
	if (!_initialized) {
		init();
	}

	return _ones[val];
}

void
fatal(char *msg)
{
	fprintf(stderr, "[FATAL] %s\n", msg);
	exit(-1);
}

char*
gen_fname(int apid)
{
	char *ret, *tmp;
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);

	tmp = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM"));
	strftime(tmp, sizeof("LRPT_YYYY_MM_DD_HH-MM"),
			 "LRPT_%Y_%m_%d-%H_%M", tm);

	if (apid < 0) {
		ret = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM.png"));
		sprintf(ret, "%s.png", tmp);
	} else {
		ret = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM-AA.png"));
		sprintf(ret, "%s-%02d.png", tmp, apid);
	}

	free(tmp);

	return ret;
}

/* Parse the APID list */
void
parse_apids(int *apid_list, char *raw)
{
	sscanf(raw, "%d,%d,%d", apid_list, apid_list+1, apid_list+2);
}

/* Malloc with abort on error */
void*
safealloc(size_t size)
{
	void *ptr;
	ptr = malloc(size);
	if (!ptr) {
		fatal("Failed to allocate block");
	}

	return ptr;
}

void
splash()
{
	fprintf(stderr, "\nLRPT decoder v%s\n\n", VERSION);
}

char*
timeofday(unsigned int msec)
{
	int hr, min, sec, ms;

	ms =  msec % 1000;
	sec = msec / 1000 % 60;
	min = msec / 1000 / 60 % 60;
	hr =  msec / 1000 / 60 / 60 % 24;

	sprintf(_time_of_day, "%02d:%02d:%02d.%03d", hr, min, sec, ms);

	return _time_of_day;
}


void
usage(const char *pname)
{
	splash();
	fprintf(stderr, "Usage: %s [options] file_in\n", pname);
	fprintf(stderr,
			"   -a, --apid R,G,B        Specify APIDs to parse (default: 68,65,64)\n"
			"   -d, --diff              Differentially decode (e.g. for Metebr-M N2-2)\n"
			"   -o, --output <file>     Output composite png to <file>\n"
			"   -q, --quiet             Only dispaly overall decoding stats\n"
			"   -s, --statfile          Write the .stat file containing timing data\n"
			"\n"
			"   -h, --help              Print this help screen\n"
			"   -v, --version           Print version info\n"
		   );
	exit(0);
}

void
version()
{
	fprintf(stderr, "LRPT decoder v%s\n", VERSION);
	fprintf(stderr, "Released under the GNU GPLv3\n\n");
	exit(0);
}

/* Static functions {{{ */
static void
init()
{
	int i, j, ones;

	for (i=0; i<255; i++) {
		_rot_lut[i] = ((i & 0x55) ^ 0x55) << 1 | (i & 0xAA) >> 1;
		_inv_lut[i] = (i & 0x55) << 1 | (i & 0xAA) >> 1;

		ones = 0;
		for (j=0; j<8; j++) {
			ones += (i >> j) & 0x01;
		}

		_ones[i] = ones;
	}
	_initialized = 1;
}
/*}}}*/

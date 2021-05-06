#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

#ifndef VERSION
#define VERSION "(unknown version)"
#endif

void
soft_to_hard(uint8_t *restrict hard, int8_t *restrict soft, int len)
{
	int i;

	assert(!(len & 0x7));

	while (len > 0) {
		*hard = 0;

		for (i=7; i>=0; i--) {
			*hard |= (*soft < 0) << i;
			soft++;
		}

		hard++;
		len -= 8;
	}
}

void
soft_derotate(int8_t *soft, int len, enum phase phase)
{
	int8_t tmp;

	/* Prevent overflows when changing sign */
	for (int i=0; i<len; i++){
		soft[i] = MAX(-127, soft[i]);
	}

	switch (phase) {
		case PHASE_0:
			break;
		case PHASE_270:
			/* (x, y) -> (-y, x) */
			for (; len>0; len-=2) {
				tmp = *soft;
				*soft = -*(soft+1);
				*(soft+1) = tmp;
				soft += 2;
			}
			break;
		case PHASE_180:
			/* (x, y) -> (-x, -y) */
			for (; len>0; len--) {
				*soft = -*soft;
				soft++;
			}
			break;
		case PHASE_90:
			/* (x, y) -> (y, -x) */
			for (; len>0; len-=2) {
				tmp = *soft;
				*soft = *(soft+1);
				*(soft+1) = -tmp;
				soft += 2;
			}
			break;
		default:
			assert(0);
			break;
	}
}

int
count_ones(uint64_t v) {
	int count;
	for (count = 0; v; count++) {
		v &= v-1;
	}
	return count;
}

void
version()
{
	printf("meteor_decode v" VERSION "\n");
}

void
usage(char *execname)
{
	fprintf(stderr, "Usage: %s [options] input_s -o output_image\n", execname);
	fprintf(stderr,
			"   -7, --70               Dump APID70 data in a separate file\n"
	        "   -a, --apid R,G,B       Specify APIDs to parse (default: autodetect)\n"
	        "   -b, --batch            Batch mode (disable all non-printable characters)\n"
	        "   -d, --diff             Perform differential decoding\n"
	        "   -i, --int              Deinterleave samples (aka 80k mode)\n"
	        "   -o, --output <file>    Output composite image to <file>\n"
	        "   -q, --quiet            Disable decoder status output\n"
	        "   -s, --split            Write each APID in a separate file\n"
	        "   -t, --statfile         Write .stat file\n"
	        "\n"
	        "   -h, --help             Print this help screen\n"
	        "   -v, --version          Print version information\n"
		   );
}

uint32_t
read_bits(const uint8_t *src, int offset_bits, int bitcount)
{
	uint64_t ret = 0;

	src += (offset_bits / 8);
	offset_bits %= 8;

	ret = (*src++) & ((1<<(8-offset_bits))-1);
	bitcount -= (8-offset_bits);

	while (bitcount > 0) {
		ret = ret << 8 | *src++;
		bitcount -= 8;
	}

	ret >>= -bitcount;
	return ret;
}

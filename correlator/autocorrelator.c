#include <stdio.h>
#include <assert.h>
#include "autocorrelator.h"

static const uint8_t _syncwords[] = {0x27, 0x4E, 0xD8, 0xB1};

int
autocorrelate(enum phase *rotation, int period, uint8_t *restrict hard, int len)
{
	int i, j, k;
	uint8_t tmp, xor, window;
	int ones_count[8*period];
	int average_bit[8*period];
	int corr, best_corr, best_idx;

	/* Make len a multiple of the period */
	len -= len % period;

	for (i=0; i<(int)LEN(ones_count); i++) {
		ones_count[i] = 0;
		average_bit[i] = 0;
	}

	/* XOR the bitstream with a delayed version of itself */
	for (i=0; i<period; i++) {
		j = len - period + i - 1;
		tmp = hard[j];
		for (j -= period; j >= 0; j -= period) {
			xor = hard[j] ^ tmp;
			tmp = hard[j];
			hard[j] = xor;

			/* Keep track of the average value of each bit in the period window */
			for (k=0; k<8; k++) {
				average_bit[8*i + 7-k] += tmp & (1<<k) ? 1 : -1;
			}
		}
	}


	/* Find the bit offset with the most zeroes */
	window = 0;
	hard--;
	for (i=0; i<8*(len-period); i++) {
		if (!(i%8)) hard++;
		window = (window >> 1) | ((*hard << (i%8)) & 0x80);
		ones_count[i % (8*period)] += count_ones(window);
	}

	best_idx = 0;
	best_corr = ones_count[0] * 9 / 10;
	for (i=1; i<(int)LEN(ones_count); i++) {
		if (ones_count[i] < best_corr) {
			best_corr = ones_count[i];
			best_idx = i;
		}
	}

	/* Collect the average syncword bits */
	tmp = 0;
	for (i=7; i>=0; i--) {
		tmp |= (average_bit[best_idx+i] > 0 ? 1<<i : 0);
	}

	/* Find the phase rotation of the syncword */
	*rotation = 0;
	best_corr = count_ones(tmp ^ _syncwords[0]);
	for (i=1; i<(int)LEN(_syncwords); i++) {
		corr = count_ones(tmp ^ _syncwords[i]);
		if (best_corr > corr) {
			best_corr = corr;
			*rotation = i;
		}
	}


	return best_idx;
}

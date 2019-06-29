#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "huffman.h"
#include "utils.h"

static int  get_quant(int quality, int x, int y);
static void dct_inverse(int16_t dst[8][8], int16_t src[8][8]);
static void dequantize(int16_t dst[8][8], int16_t src[8][8], int quality);
static void unzigzag(int16_t block[8][8]);

/* Quantization table, standard 50% quality JPEG */
static const int _quant[8][8] =
{
	{16, 11, 10, 16, 24, 40, 51, 61},
	{12, 12, 14, 19, 26, 58, 60, 55},
	{14, 13, 16, 24, 40, 57, 69, 56},
	{14, 17, 22, 29, 51, 87, 80, 62},
	{18, 22, 37, 56, 68, 109,103,77},
	{24, 35, 55, 64, 81, 104,113,92},
	{49, 64, 78, 87, 103,121,120,101},
	{72, 92, 95, 98, 112,100,103,99}
};

/* 8x8 reverse zigzag pattern */
static const int _zigzag_lut[64] =
{
	0,  1,  8,  16, 9,  2,  3,  10,
	17, 24, 32, 25, 18, 11, 4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13, 6,  7,  14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};


static float _cos_lut[8][8];
static float _alpha_lut[8];
static int _initialized = 0;


/* Initialze cosine/alpha lookup tables */
void
jpeg_init()
{
	int i, j;

	if (!_initialized) {
		for (i=0; i<8; i++) {
			_alpha_lut[i] = (i == 0 ? 1/M_SQRT2 : 1);
			for (j=0; j<8; j++) {
				_cos_lut[i][j] = cosf((2*i+1)*M_PI*j/16);
			}
		}
		_initialized = 1;
	}
}

/* Decode an 8x8 block */
int
jpeg_decode(uint8_t dst[8][8], int16_t src[8][8], int quality)
{
	int i, j;
	int16_t tmp[8][8];

	unzigzag(src);
	dequantize(tmp, src, quality);
	dct_inverse(src, tmp);

	/* Renormalize to (0, 255) */
	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			dst[i][j] = MIN(255, MAX(0, src[i][j] + 128));
		}
	}

	return 0;
}

/* Static functions {{{ */
/* Entrywise product with the quantization matrix. Might return values higher
 * than 255, so int16_t it is */
static void
dequantize(int16_t dst[8][8], int16_t src[8][8], int quality)
{
	int i, j;

	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			dst[i][j] = src[i][j] * get_quant(quality, i, j);
		}
	}
}

/* Inverse discrete cosine transform on an 8x8 src */
static void
dct_inverse(int16_t dst[8][8], int16_t src[8][8])
{
	int i, j, u, v;
	float tmp;

	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			tmp = 0;;
			for (u=0; u<8; u++) {
				for (v=0; v<8; v++) {
					tmp += _alpha_lut[u] * _alpha_lut[v] * src[u][v] *
					       _cos_lut[i][u] * _cos_lut[j][v];
				}
			}
			dst[i][j] = round(tmp / 4);
		}
	}
}

/* Compute quantization values from the quality factor specified in the MTU */
static int
get_quant(int quality, int x, int y)
{
	float compr_ratio;

	if (quality < 50) {
		compr_ratio = 5000.0 / quality;
	} else {
		compr_ratio = 200 - 2*quality;
	}

	return MAX(1, round(_quant[x][y] * compr_ratio/100.0));
}

/* Descramble a jpeg 8x8 block */
static void
unzigzag(int16_t block[8][8])
{
	int i, j;
	int16_t tmp[64];

	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			tmp[_zigzag_lut[i*8+j]] = block[i][j];
		}
	}

	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			block[i][j] = tmp[i*8+j];
		}
	}
}
/*}}}*/

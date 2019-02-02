#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "utils.h"

static int  get_quant(int quality, int x, int y);
static void dct_inverse(int16_t dst[8][8], int16_t src[8][8]);
static void dequantize(int16_t dst[8][8], int16_t src[8][8], int quality);

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
/*	{16,12,14,14,18,24,49,72},*/
/*	{11,12,13,17,22,35,64,92},*/
/*	{10,14,16,22,37,55,78,95},*/
/*	{16,19,24,29,56,64,87,98},*/
/*	{24,26,40,51,68,81,103,112},*/
/*	{40,58,57,87,109,104,121,100},*/
/*	{51,68,69,80,103,113,120,103},*/
/*	{61,55,56,62,77,92,101,99},*/

};

static float _cos_lut[8][8];
static float _alpha_lut[8];
static int _initialized = 0;

void
jpeg_init()
{
	int i, j;

	/* Initialze cosine/alpha lookup tables */
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

	dequantize(tmp, src, quality);
	dct_inverse(src, tmp);

	/* Renormalize from (-512, 511) to (0, 255) */
	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			dst[i][j] = MAX(0, (src[i][j]) + 128);
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

/* In-place inverse discrete cosine transform on an 8x8 src */
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

	return MAX(1, round(_quant[x][y] * (float)compr_ratio/100));
}
/*}}}*/

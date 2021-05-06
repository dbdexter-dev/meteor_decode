#include "jpeg.h"
#include "utils.h"
#define Q_SHIFT 14

static void unzigzag(int16_t block[8][8]);
static int quantization(int quality, int x, int y);
static void dequantize(int16_t block[8][8], int quality);
static void inverse_dct(uint8_t dst[8][8], int16_t src[8][8]);

/* Quantization table, standard 50% quality JPEG */
static const uint8_t _quant[8][8] =
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
static const uint8_t _zigzag_lut[64] =
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

/* Cosine lookup table, Q14 */
static const int16_t _cos_lut[8][8] =
{
	{0x4000, 0x3ec5, 0x3b21, 0x3537, 0x2d41, 0x238e, 0x187e, 0x0c7c},
	{0x4000, 0x3537, 0x187e, (int16_t)0xf384, (int16_t)0xd2bf, (int16_t)0xc13b, (int16_t)0xc4df, (int16_t)0xdc72},
	{0x4000, 0x238e, (int16_t)0xe782, (int16_t)0xc13b, (int16_t)0xd2bf, 0x0c7c, 0x3b21, 0x3537},
	{0x4000, 0x0c7c, (int16_t)0xc4df, (int16_t)0xdc72, 0x2d41, 0x3537, (int16_t)0xe782, (int16_t)0xc13b},
	{0x4000, (int16_t)0xf384, (int16_t)0xc4df, 0x238e, 0x2d41, (int16_t)0xcac9, (int16_t)0xe782, 0x3ec5},
	{0x4000, (int16_t)0xdc72, (int16_t)0xe782, 0x3ec5, (int16_t)0xd2bf, (int16_t)0xf384, 0x3b21, (int16_t)0xcac9},
	{0x4000, (int16_t)0xcac9, 0x187e, 0x0c7c, (int16_t)0xd2bf, 0x3ec5, (int16_t)0xc4df, 0x238e},
	{0x4000, (int16_t)0xc13b, 0x3b21, (int16_t)0xcac9, 0x2d41, (int16_t)0xdc72, 0x187e, (int16_t)0xf384},
};

/* Alpha lookup table, Q14 */
static const int16_t _alpha_lut[8][8] =
{
	{0x2000, 0x2d41, 0x2d41, 0x2d41, 0x2d41, 0x2d41, 0x2d41, 0x2d41},
	{0x2d41, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000},
	{0x2d41, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000},
	{0x2d41, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000},
	{0x2d41, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000},
	{0x2d41, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000},
	{0x2d41, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000},
	{0x2d41, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000},
};

void
jpeg_decode(uint8_t dst[8][8], int16_t src[8][8], int q)
{
	static int last_q;

	/* Try to generate a semi-valid strip if q=0 (happens on overflows of M2) */
	q = q > 0 ? q : last_q;
	last_q = q;

	unzigzag(src);
	dequantize(src, q);
	inverse_dct(dst, src);
}

/* Static functions {{{ */
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

static void
dequantize(int16_t block[8][8], int quality)
{
	int i, j;

	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			block[i][j] = ((int32_t)block[i][j] * quantization(quality, i, j));
		}
	}
}

static void
inverse_dct(uint8_t dst[8][8], int16_t src[8][8])
{
	int i, j, u, v;
	int32_t tmp;

	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			tmp = 0;
			for (u=0; u<8; u++) {
				for (v=0; v<8; v++) {
					/* Happens quite often in JPEGs */
					if (src[u][v] == 0) {
						continue;
					}
					tmp += ((_alpha_lut[u][v]
							* (((int32_t)_cos_lut[i][u] * (int32_t)_cos_lut[j][v]) >> Q_SHIFT)
						   ) >> Q_SHIFT) * src[u][v];
				}
			}
			dst[i][j] = MAX(0, MIN(255, ((tmp/4 >> Q_SHIFT) + 128)));
		}
	}
}

static int
quantization(int quality, int x, int y)
{
	int compr_ratio;
	if (quality < 50) {
		compr_ratio = 5000 / quality;
	} else {
		compr_ratio = 200 - 2*quality;
	}

	return MAX(1, (((int)_quant[x][y] * compr_ratio / 50) + 1) / 2);
}
/* }}} */

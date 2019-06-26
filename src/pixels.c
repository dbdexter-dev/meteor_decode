#include <stdint.h>
#include "huffman.h"
#include "jpeg.h"
#include "packet.h"
#include "packetizer.h"
#include "pixels.h"
#include "utils.h"

static void unzigzag(int16_t block[8][8]);

static uint8_t _black_thumbnail[8][8] = { 0 };
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

PixelGen*
pixelgen_init(BmpSink *s, BmpChannel chan)
{
	PixelGen *ret;

	ret = safealloc(sizeof(*ret));
	ret->bmp = s;
	ret->channel = chan;

	ret->mcu_nr = 0;
	ret->pkt_end = -1;

	huffman_init();
	jpeg_init();

	return ret;
}

void
pixelgen_deinit(PixelGen *self)
{
	free(self);
}

/* Append a 14 x 8x8 strip to one of the channels of a BmpSink */
void
pixelgen_append(PixelGen *self, const Segment *seg)
{
	Mcu *mcu;
	int i;
	int quality;
	const uint8_t *raw_data;
	int16_t decoded_strip[MCU_PER_MPDU][8][8];
	uint8_t thumbnail[8][8];

	if (!self) {
		return;
	}

	if (!seg) {
		for (i=0; i<MCU_PER_MPDU; i++) {
			bmp_append(self->bmp, _black_thumbnail, self->channel);
		}
		self->mcu_nr = (self->mcu_nr + MCU_PER_MPDU) % MCU_PER_PP;
	} else {
		mcu = (Mcu*)seg->data;
		quality = mcu_quality_factor(mcu);
		raw_data = mcu_data_ptr(mcu);

		/* Append black if Huffman decoding fails */
		if (huffman_decode(decoded_strip, raw_data, MCU_PER_MPDU, seg->len)<0) {
			pixelgen_append(self, NULL);
			return;
		}

		/* Transform the data into a pixel matrix, and append it to the bmp */
		for (i=0; i<MCU_PER_MPDU; i++) {
			unzigzag(decoded_strip[i]);
			jpeg_decode(thumbnail, decoded_strip[i], quality);
			bmp_append(self->bmp, thumbnail, self->channel);
		}

		/* Update the next expected MCU number and segment end */
		self->mcu_nr = (self->mcu_nr + MCU_PER_MPDU) % MCU_PER_PP;
		if (self->mcu_nr) {
			self->pkt_end = (seg->seq + (MCU_PER_PP - self->mcu_nr)/MCU_PER_MPDU) % MPDU_MAX_SEQ;
		} else {
			self->pkt_end = (seg->seq + 3*MPDU_PER_LINE+1) % MPDU_MAX_SEQ;
		}
	}

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
/*}}}*/

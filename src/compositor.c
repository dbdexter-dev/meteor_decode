#include <stdint.h>
#include "bmp.h"
#include "compositor.h"
#include "huffman.h"
#include "jpeg.h"
#include "packetizer.h"
#include "utils.h"

static void unzigzag(int16_t block[8][8]);

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

static uint8_t _black_square[8][8] = { 0 };


Compositor*
comp_init(BmpSink *s, int init_offset)
{
	Compositor *ret;

	ret = safealloc(sizeof(*ret));
	ret->bmp = s;
	ret->next_mcu_seq = init_offset;
	jpeg_init();
	huffman_init();

	return ret;
}

void
comp_deinit(Compositor *self)
{
	free(self);
}

/* Decode the 14 MCUs inside a segment and send them to a BmpSink */
int
comp_compose(Compositor *self, const Segment *seg)
{
	Mcu *mcu;
	int i, j;
	int jpeg_quality, seq_delta;
	int bytes_read;
	const uint8_t *raw_data;
	int16_t decoded_strip[MCU_PER_MPDU][8][8];
	uint8_t tmp[8][8];

	/* len=0 means packet wasn't properly decoded: pad the bmp with black pixels
	 * for the length of a partial packet and exit */
	if (!seg->len) {
		for (i=0; i<MCU_PER_PP; i++) {
			bmp_append(self->bmp, _black_square);
		}
		return 0;
	}

	mcu = (Mcu*)seg->data;
	jpeg_quality = mcu_quality_factor(mcu);
	raw_data = mcu_data_ptr(mcu);
	seq_delta = (mcu_seq(mcu) - self->next_mcu_seq + MCU_PER_PP) % MCU_PER_PP;

	if (seq_delta) {
		printf("Lost %d MCUs\n", seq_delta);
	}
	/* Realign with black squares to account for the lost MCUs */
	for (i = (seq_delta + MCU_PER_PP) % MCU_PER_PP; i>0; i--) {
		bmp_append(self->bmp, _black_square);
	}

	/* Huffman-decode the whole strip */
	bytes_read = huffman_decode(decoded_strip, raw_data, MCU_PER_MPDU);
	if (seg->len <= bytes_read) {
		printf("[WARN] possible bytes buffer overrun? %d <-> %d\n", seg->len, bytes_read);
	}

	/* Un-zigzag, decompress, and write out each block */
	for (i=0; i<MCU_PER_MPDU; i++) {
		unzigzag(decoded_strip[i]);
		jpeg_decode(tmp, decoded_strip[i], jpeg_quality);
		if (i==0) {
			for (j=0; j<8; j++) {
				tmp[j][0] = 0xFF;
			}
		}
		bmp_append(self->bmp, tmp);
	}

	self->next_mcu_seq = (mcu_seq(mcu) + MCU_PER_MPDU) % (MCU_PER_PP);

	return 0;
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

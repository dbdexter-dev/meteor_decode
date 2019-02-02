#include <stdint.h>
#include "bmp.h"
#include "compositor.h"
#include "huffman.h"
#include "jpeg.h"
#include "packetizer.h"
#include "utils.h"

static void unzigzag(int16_t block[8][8]);

/* 8x8 reverse zigzag pattern */
const int _zigzag_lut[64] = 
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


Compositor*
comp_init(BmpSink *s)
{
	Compositor *ret;

	ret = safealloc(sizeof(*ret));
	ret->bmp = s;
	jpeg_init();

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
	int i;
	int jpeg_quality;
	const uint8_t *raw_data;
	int16_t decoded_strip[MCU_PER_MPDU][8][8];
	uint8_t unsigned_tmp[8][8];

	jpeg_quality = mcu_quality_factor((Mcu*)seg->data);
	if (seg->has_sec_hdr) {
		raw_data = seg->data + MCU_HDR_SIZE;
	} else {
		raw_data = seg->data;
	}

	/* Decode the whole strip */
	printf("Composing %d bytes\n", seg->len - MCU_HDR_SIZE);
	huffman_decode(decoded_strip, raw_data, MCU_PER_MPDU);

	/* Un-zigzag, decompress, and write out each block */
	/* TODO handle missing strips/blocks */
	for (i=0; i<MCU_PER_MPDU; i++) {
		unzigzag(decoded_strip[i]);
		jpeg_decode(unsigned_tmp, decoded_strip[i], jpeg_quality);
		bmp_append_block(self->bmp, unsigned_tmp);
	}

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

#include <stdint.h>
#include "huffman.h"
#include "jpeg.h"
#include "packet.h"
#include "packetizer.h"
#include "pixels.h"
#include "utils.h"

static uint8_t _black_thumbnail[8][8] = { 0 };

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

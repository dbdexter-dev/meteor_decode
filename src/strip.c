#include <string.h>
#include "huffman.h"
#include "jpeg.h"
#include "packet.h"
#include "packetizer.h"
#include "strip.h"
#include "utils.h"

Channel*
channel_init(int apid)
{
	Channel *ret = safealloc(sizeof(*ret));

	ret->apid = apid;
	ret->last_seq = -1;
	ret->mcu_offset = 0;
	ret->len = MCU_PER_PP;
	ret->data = safealloc(ret->len);
	ret->ptr = ret->data;

	huffman_init();
	jpeg_init();

	return ret;
}

void
channel_deinit(Channel *self)
{
	free(self->data);
	self->ptr = NULL;
}

void
channel_decode(Channel *self, const Segment *seg)
{
	Mcu *mcu;
	int i;
	int quality;
	size_t offset;
	int16_t decoded_strip[MCU_PER_MPDU][8][8];
	uint8_t thumbnail[8][8];
	const uint8_t *raw_data;

	if (!self) {
		return;
	}

	/* Alloc space for another line (if needed) */
	offset = channel_size(self);
	if (offset + MCU_PER_MPDU * 64 > self->len) {
		self->len += MCU_PER_PP * 64;
		self->data = realloc(self->data, self->len);
		self->ptr = self->data + offset;
	}

	if (!seg) {
		memset(self->ptr, '\0', MCU_PER_MPDU * 64);
		self->ptr += MCU_PER_MPDU * 64;
	} else {
		mcu = (Mcu*)seg->data;
		quality = mcu_quality_factor(mcu);
		raw_data = mcu_data_ptr(mcu);

		if (huffman_decode(decoded_strip, raw_data, MCU_PER_MPDU, seg->len) < 0) {
			channel_decode(self, NULL);
			return;
		}

		for (i=0; i<MCU_PER_MPDU; i++) {
			jpeg_decode(thumbnail, decoded_strip[i], quality);
			memcpy(self->ptr, thumbnail, 64);
			self->ptr += 64;
		}

		self->last_seq = seg->seq;
		self->end_seq = (seg->seq + ((MCU_PER_PP - self->mcu_offset) / MCU_PER_MPDU));
	}
	self->mcu_offset = (self->mcu_offset + MCU_PER_MPDU) % MCU_PER_PP;
}

/* Fill an image up until the whole line we are working on is black */
void
channel_nextline(Channel *self)
{
	do {
		channel_decode(self, NULL);
	} while (self->mcu_offset > 0);
}

size_t
channel_size(Channel *self)
{
	return self->ptr - self->data;
}

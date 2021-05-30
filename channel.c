#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "channel.h"
#include "protocol/mpdu.h"
#include "protocol/mcu.h"
#include "utils.h"

#define PIXELS_PER_STRIP (MCU_PER_LINE*8*8)

static const uint8_t black_strip[MCU_PER_MPDU][8][8];
static void cache_strip(Channel *ch, const uint8_t (*strip)[8][8]);

int
channel_init(Channel *ch, int apid)
{
	ch->mcu_seq = 0;
	ch->mpdu_seq = -1;
	ch->apid = apid;

	ch->offset = 0;
	ch->len = STRIPS_PER_ALLOC * PIXELS_PER_STRIP;
	ch->pixels = malloc(ch->len);

	return 0;
}

void
channel_close(Channel *ch)
{
	if (ch->pixels) {
		free(ch->pixels);
		ch->pixels = NULL;
	}
}

void
channel_append_strip(Channel *ch, const uint8_t (*strip)[8][8], unsigned int mcu_seq, unsigned int mpdu_seq)
{
	int i;
	int mpdu_delta, mcu_delta;
	int lines_lost, strips_lost;

	/* Handle misalignments. Can occur after a satelliteb buffer overflow */
	mcu_seq -= (mcu_seq % MCU_PER_MPDU);

	/* Align image to the given MPDU and MCU sequence numbers */
	mpdu_delta = (mpdu_seq - ch->mpdu_seq - 1 + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;
	mcu_delta = (mcu_seq - ch->mcu_seq + MCU_PER_LINE) % MCU_PER_LINE;

	lines_lost = (ch->mpdu_seq < 0) ? 0 : mpdu_delta / MPDU_PER_PERIOD;
	strips_lost = mcu_delta / MCU_PER_MPDU + lines_lost * MPDU_PER_LINE;

	/* Align images and update current mpdu seq counter */
	for (i = strips_lost; i>0; i--) {
		cache_strip(ch, black_strip);
	}
	ch->mpdu_seq = mpdu_seq;
	ch->mcu_seq = mcu_seq;

	/* Copy strip into cache */
	cache_strip(ch, strip ? strip : black_strip);
}

static void
cache_strip(Channel *ch, const uint8_t (*strip)[8][8])
{
	int row, block;
	unsigned int old_len;

	/* If this write would go out of bounds, allocate more memory and initialize
	 * it to black pixels */
	if (ch->offset + PIXELS_PER_STRIP > ch->len) {
		old_len = ch->len;
		ch->len += STRIPS_PER_ALLOC*PIXELS_PER_STRIP;

		ch->pixels = realloc(ch->pixels, ch->len);
		memset(ch->pixels+old_len, '\0', ch->len - old_len);
	}

	if (!ch->pixels) return;

	/* Reshape strip and add to the local pixel array */
	for (row=0; row<8; row++) {
		for (block=0; block<MCU_PER_MPDU; block++) {
			memcpy(&ch->pixels[ch->offset + row*MCU_PER_LINE*8 + (ch->mcu_seq+block)*8], strip[block][row], 8);
		}
	}

	ch->mcu_seq += MCU_PER_MPDU;
	if (ch->mcu_seq >= MCU_PER_LINE) {
		/* Advance both the MCU seq (rolling over) and the MPDU seq (moving
		 * forward by the frame period minus this channel's size). Rollover
		 * isn't really necessary here because channel_append_strip() already
		 * computes the difference modulo MPDU_MAX_SEQ. */
		ch->mcu_seq = 0;
		ch->mpdu_seq += MPDU_PER_PERIOD - MPDU_PER_LINE;
		ch->offset += PIXELS_PER_STRIP;
	}
}

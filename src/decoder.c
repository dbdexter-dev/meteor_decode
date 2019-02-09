#include <stdint.h>
#include "bmp.h"
#include "decoder.h"
#include "packet.h"
#include "utils.h"
#include "viterbi.h"

static void* decoder_thr_run(void *arg);

Decoder*
decoder_init(BmpSink *dst, SoftSource *src, int apids[3])
{
	Decoder *ret;
	uint8_t encoded_syncword[2*sizeof(SYNCWORD)];

	ret = safealloc(sizeof(*ret));

	viterbi_encode(encoded_syncword, SYNCWORD, sizeof(SYNCWORD));

	ret->correlator = correlator_init_soft(src, encoded_syncword);
	ret->viterbi = viterbi_init(ret->correlator);
	ret->pp = pkt_init(ret->viterbi);

	ret->apids[0] = apids[0];
	ret->apids[1] = apids[1];
	ret->apids[2] = apids[2];

	ret->pxgen[0] = pixelgen_init(dst, RED);
	ret->pxgen[1] = pixelgen_init(dst, GREEN);
	ret->pxgen[2] = pixelgen_init(dst, BLUE);

	ret->running = 0;
	ret->seq = -1;
	ret->last_tstamp = 0;
	ret->apid = 0;
	ret->total_count = 0;
	ret->valid_count = 0;

	return ret;
}

void
decoder_deinit(Decoder *self)
{
	void *retval;

	/* Join the running decoder */
	self->running = 0;
	pthread_join(self->t, &retval);

	pixelgen_deinit(self->pxgen[0]);
	pixelgen_deinit(self->pxgen[1]);
	pixelgen_deinit(self->pxgen[2]);

	pkt_deinit(self->pp);
	self->viterbi->close(self->viterbi);
	self->correlator->close(self->correlator);
}

void
decoder_start(Decoder *self)
{
	self->running = 1;
	pthread_create(&self->t, NULL, decoder_thr_run, (void*)self);
}

int
decoder_get_status(Decoder *self)
{
	return self->running;
}

int
decoder_get_rs_count(Decoder *self)
{
	return self->pp->rs_fix_count;
}

int
decoder_get_apid(Decoder *self)
{
	return self->apid;
}

int
decoder_get_seq(Decoder *self)
{
	return self->seq;
}

int
decoder_get_valid_count(Decoder *self)
{
	return self->valid_count;
}

int
decoder_get_total_count(Decoder *self)
{
	return self->total_count;
}

uint32_t
decoder_get_time(Decoder *self)
{
	return self->last_tstamp;
}

uint32_t
decoder_get_syncword(Decoder *self)
{
	return ((Cadu*)self->pp->cadu)->sync_marker;
}

/* Static functions {{{ */
static void*
decoder_thr_run(void *arg)
{
	int mcu_nr, seq_delta;
	int align_okay;
	int i, ch;
	Mcu *mcu;
	Segment seg;
	Decoder *self;
	PixelGen *cur_gen;

	self = (Decoder*)arg;

	while (self->running && pkt_read(self->pp, &seg)) {
		self->total_count++;
		/* Skip invalid packets */
		if (seg.len <= 0) {
			continue;
		}
		self->valid_count++;

		/* Skip packets that are not images from the AVHRR */
		if (seg.apid < 64 || seg.apid >= 70) {
			continue;
		}

		mcu = (Mcu*)seg.data;
		mcu_nr = mcu_seq(mcu);
		seq_delta = (seg.seq - self->seq + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;

		/* Compensate for lost MCUs */
		if (self->seq > 0 && seq_delta > 1) {
			align_okay = 0;
			while (!align_okay) {
				for (ch=0; ch<3; ch++) {
					cur_gen = self->pxgen[ch];
					if (cur_gen->pkt_end < seg.seq || cur_gen->pkt_end - seg.seq > MPDU_MAX_SEQ/2) {
						for (i=cur_gen->mcu_nr; i<MCU_PER_PP; i += MCU_PER_MPDU) {
							pixelgen_append(cur_gen, NULL);
						}
						cur_gen->mcu_nr = 0;
						cur_gen->pkt_end = (cur_gen->pkt_end + 3*MPDU_PER_LINE+1) % MPDU_MAX_SEQ;
					} else {
						align_okay = 1;
					}
				}
			}
		}

		self->apid = seg.apid;
		self->last_tstamp = seg.timestamp;

		/* Append the received MCU */
		for (ch=0; ch<3; ch++) {
			if (seg.apid == self->apids[ch]) {
				cur_gen = self->pxgen[ch];
				for (i = cur_gen->mcu_nr; i < mcu_nr; i += MCU_PER_MPDU) {
					pixelgen_append(cur_gen, NULL);
				}
				pixelgen_append(cur_gen, &seg);
			}
		}

		self->seq = seg.seq;
	}

	self->running = 0;
	return NULL;
}
/* }}} */

#ifndef LRPTDEC_DECODER_H
#define LRPTDEC_DECODER_H

#include <pthread.h>
#include "bmp.h"
#include "correlator.h"
#include "packetizer.h"
#include "pixels.h"
#include "viterbi.h"

typedef struct {
	SoftSource *src, *correlator;
	HardSource *viterbi;
	Packetizer *pp;
	PixelGen *pxgen[3];
	int apids[3];

	pthread_t t;
	volatile int running;
	int seq, apid;
	uint32_t last_tstamp;
} Decoder;

Decoder *decoder_init(BmpSink *dst, SoftSource *src, int apids[3]);
void     decoder_deinit(Decoder *self);
void     decoder_start(Decoder *self);

int      decoder_get_status(Decoder *self);
int      decoder_get_rs_count(Decoder *self);
int      decoder_get_apid(Decoder *self);
uint32_t decoder_get_time(Decoder *self);
uint32_t decoder_get_syncword(Decoder *self);

#endif

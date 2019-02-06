/**
 * Append pixels to a BmpSink channel, one MPDU (14 8x8 thumbnails) at a time
 */
#ifndef LRPTDEC_PIXELS_H
#define LRPTDEC_PIXELS_H

#include "bmp.h"
#include "packetizer.h"

typedef struct {
	BmpSink *bmp;
	BmpChannel channel;
	int mcu_nr;
	int pkt_end;
} PixelGen;

PixelGen *pixelgen_init(BmpSink *s, BmpChannel chan);
void      pixelgen_deinit(PixelGen *m);

void pixelgen_append(PixelGen *self, const Segment *seg);

#endif

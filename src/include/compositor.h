/**
 * Glue between packets and images: given a segment, it'll call the appropriate
 * functions to Huffman decode its data, decompress the resulting JPEG
 * thumbnails, and write those thumbnails to a BMP 
 */
#ifndef LRPTDEC_COMPOSITOR_H
#define LRPTDEC_COMPOSITOR_H

#include "bmp.h"
#include "packetizer.h"

typedef struct {
	BmpSink *bmp;
	int next_mcu_seq;
} Compositor;

Compositor* comp_init(BmpSink *s, int init_offset);
int         comp_compose(Compositor *c, const Segment *seg);
void        comp_deinit(Compositor *c);

#endif


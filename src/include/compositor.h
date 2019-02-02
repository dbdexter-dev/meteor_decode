#ifndef LRPTDEC_COMPOSITOR_H
#define LRPTDEC_COMPOSITOR_H

#include "bmp.h"
#include "packetizer.h"

typedef struct {
	BmpSink *bmp;
} Compositor;

Compositor* comp_init(BmpSink *s);
int         comp_compose(Compositor *c, const Segment *seg);
void        comp_deinit(Compositor *c);

#endif


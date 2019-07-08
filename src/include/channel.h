#ifndef LRPTDEC_CHANNEL_H
#define LRPTDEC_CHANNEL_H

#include <stdint.h>
#include <stdlib.h>
#include "packetizer.h"

typedef struct {
	int apid;
	int last_seq;
	int end_seq;
	int mcu_offset;
	size_t len;
	uint8_t *data;
	uint8_t *ptr;
} Channel;


Channel* channel_init(int apid);
void     channel_deinit(Channel *self);
void     channel_decode(Channel *self, const Segment *seg);
void     channel_newline(Channel *self);
size_t   channel_size(Channel *self);

#endif

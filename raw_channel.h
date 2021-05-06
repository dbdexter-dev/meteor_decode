#ifndef raw_channel_h
#define raw_channel_h

#include <stdio.h>
#include <stdint.h>

typedef struct {
	FILE *fd;
} RawChannel;

void raw_channel_init(RawChannel *ch, const char *fname);
void raw_channel_write(RawChannel *ch, uint8_t *data, int len);
void raw_channel_close(RawChannel *ch);


#endif

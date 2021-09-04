#ifndef raw_channel_h
#define raw_channel_h

#include <stdio.h>
#include <stdint.h>

typedef struct {
	FILE *fd;
} RawChannel;

/**
 * Initialize a raw channel object
 *
 * @param ch the channel to initalize
 * @param fname fname the file to dump the raw data to
 * @return 0 on success, non-zero on failure
 */
int raw_channel_init(RawChannel *ch, const char *fname);

/**
 * Finalize a raw channel
 *
 * @param ch the channel to close
 */
void raw_channel_close(RawChannel *ch);


/**
 * Write data to a raw channel
 *
 * @param ch the channel to write the data to
 * @param data pointer to the data to write
 * @param len the length of the data buffer to write
 */
void raw_channel_write(RawChannel *ch, uint8_t *data, int len);


#endif

#ifndef channel_h
#define channel_h

#include <stdio.h>
#include <stdint.h>
#include "protocol/mpdu.h"

#define STRIPS_PER_ALLOC 32

typedef struct {
	int mcu_seq, mpdu_seq;
	uint8_t *pixels;
	unsigned long offset, len;
	int apid;
} Channel;

/**
 * Initialize a Channel object
 *
 * @param ch the channel to initialize
 * @param apid APID associated with this channel
 * @return 0 on success
 *         anything else on failure
 */
int  channel_init(Channel *ch, int apid);

/**
 * Finalize a channel, flushing any internal caches and closing the output file
 *
 * @param ch the channel to close
 */
void channel_close(Channel *ch);

/**
 * Append a (8n)x8 strip to the current channel, compensating for lost strips
 * based on the MCU sequence number and the MPDU sequence number.
 *
 * @param ch the channel to append the strip to
 * @param strip pointer to multiple 8x8 blocks to write
 * @param mcu_seq sequence number associated with the strip
 * @param mpdu_seq sequence number associated with the MPDU the strip was in
 */
void channel_append_strip(Channel *ch, const uint8_t (*strip)[8][8], unsigned int mcu_seq, unsigned int mpdu_seq);

#endif /* channel_h */

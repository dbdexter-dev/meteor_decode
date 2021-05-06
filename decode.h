#ifndef decode_h
#define decode_h

#include <stdint.h>
#include <stdlib.h>
#include "protocol/mpdu.h"

typedef enum {
	EOF_REACHED=0, NOT_READY, MPDU_READY, STATS_ONLY
} DecoderState;

/**
 * Initialize the decoder's internal structures
 *
 * @param diffcoded 1 if the samples are differentially coded, 0 otherwise
 * @param interleaved 1 if the samples are interleaved (80k mode), 0 otherwise
 */
void decode_init(int diffcoded, int interleaved);

/**
 * Fetch a CADU usinge the given function pointer, and decode all the valid
 * MPDUs in it.
 *
 * @param dst pointer to the destination MPDU buffer
 * @param read_samples function to use to fetch new soft samples
 *
 * @return EOF_REACHED if a call to read_samples returned 0 bytes
 *         NOT_READY if more processing is required before a MPDU is ready
 *         MPDU_READY if dst was updated with a new MPDU
 *         STATS_ONLY if no MPDU was read, but the internal statistics were
 *                    updated
 *
 */
DecoderState decode_soft_cadu(Mpdu *dst, int(*read_samples)(int8_t *dst, size_t len));


/**
 * Various accessors to private decoder data: Reed-solomon errors, average
 * Viterbi cost, VCDU sequence number
 */
int decode_get_rs();
int decode_get_vit();
uint32_t decode_get_vcdu_seq();

#endif

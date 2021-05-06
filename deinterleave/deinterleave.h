#ifndef deinterleave_h
#define deinterleave_h

#define INTER_MARKER 0x27
#define INTER_MARKER_STRIDE 80
#define INTER_MARKER_INTERSAMPS (INTER_MARKER_STRIDE - 8)

#define INTER_BRANCH_COUNT 36
#define INTER_BRANCH_DELAY 2048

#define INTER_SIZE(x) (x*10/9+8)

#include <stdint.h>
#include <stdlib.h>

/**
 * Deinterleave a set of soft samples, and extract a corresponding number of
 * bits from the deinterleaver
 *
 * @param dst pointer to a buffer where the deinterleaved samples should be written
 * @param src pointer to the raw interleaved samples
 * @param len number of samples to read. len*72/80 bits will be written
 */
void   deinterleave(int8_t *dst, const int8_t *src, size_t len);

/**
 * Compute the number of samples to write into the deinterleaver in order to
 * obtain a certain number of samples from it
 *
 * @param output_count desired number of deinterleaved samples
 * @return number of samples to write in order to extract $output_count bits
 */
size_t deinterleave_num_samples(size_t output_count);

/**
 * Get where the deinterleaver expects the next marker to be
 *
 * @return number of samples expected before the synchronization marker
 */
int    deinterleave_expected_sync_offset();

#endif

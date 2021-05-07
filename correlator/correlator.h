#ifndef correlator_h
#define correlator_h

#include <stdint.h>
#include "utils.h"

#define CORR_THR 42     /* Empirical minimum correlation threshold for offset 0:
                         * if the correlation is > this value, we assume that
                         * the packet starts at offset 0 */

/**
 * Initialize the correlator with a synchronization sequence
 *
 * @param syncword one of the four rotations representing the synchronization
 *        word
 */
void correlator_init(uint64_t syncword);

/**
 * Look for the synchronization word inside a buffer
 *
 * @param best_phase the rotation to which the synchronization word correlates
 *        the best to. Will only be written to by the function
 * @param hard_cadu pointer to a byte buffer containing the data to correlate
 *        the syncword to
 * @param len length of the byte buffer, in bytes
 * @return the offset with the highest correlation to the syncword
 */
int  correlate(enum phase *restrict best_phase, uint8_t *restrict hard_cadu, int len);

#endif

#ifndef autocorrelator_h
#define autocorrelator_h

#include <stdint.h>
#include "utils.h"

/**
 * Correlate the given set of samples with a delayed copy of itself to find the
 * interleaving synchronization marker
 *
 * @param rotation the rotation to which the synchronization word correlates the
 *        best to. Will be only written to by the function
 * @param period distance between two synchronization markers
 * @param hard pointer to a byte buffer containing the hard samples to find the
 *        synchronization marker in
 * @param len length of the byte buffer, in bytes
 * @return the offset with the highest correlation to the syncword
 */
int autocorrelate(enum phase *rotation, int period, uint8_t *restrict hard, int len);

#endif /* autocorrelator_h */

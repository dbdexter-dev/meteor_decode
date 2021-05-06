#ifndef descramble_h
#define descramble_h

#include "protocol/cadu.h"

#define NOISE_PERIOD 255

/**
 * Initialize the descrambler
 */
void descramble_init();

/**
 * Descramble a CADU. The LFSR used to generate pseudorandom noise is
 * reinitialized to its starting state for each call.
 *
 * @param c the CADU to descramble. Descrambling happens in-place.
 */
void descramble(Cadu *c);

#endif

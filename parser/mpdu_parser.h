#ifndef mpdu_parser_h
#define mpdu_parser_h

#include <stdint.h>
#include "protocol/mpdu.h"
#include "protocol/vcdu.h"

typedef enum {
	PROCEED=0,
	FRAGMENT,
	PARSED
} ParserStatus;

/**
 * (Re-)initialize the MPDU parser.
 */
void mpdu_parser_init();

/**
 * Reconstruct an MPDU from a VCDU. Has an internal state machine that advances
 * based on the data encountered in the VCDU data unit zone. Typical usage: keep
 * calling in a loop on the same data until it returns PROCEED.
 *
 * @param dst the destination buffer to build the MPDU into
 * @param src the VCDU to process.
 * @return PROCEED if there is no data left to process inside the current VCDU
 *         FRAGMENT if some data was processed but no MPDU is available yet
 *         PARSED if a complete MPDU was reconstructed (might not be done with the VCDU though)
 */
ParserStatus mpdu_reconstruct(Mpdu *dst, Vcdu *src);

#endif /* mpdu_parser_h */

/**
 * Viterbi decoder for a (1/2, 7) convolutional coding scheme. Given a
 * convolutionally coded SoftSource*, outputs the most likely sequence that
 * originated it as a HardSource*.
 */
#ifndef LRPTDEC_VITERBI_H
#define LRPTDEC_VITERBI_H

#include <stdint.h>
#include <stdlib.h>
#include "source.h"

#define K 7
#define G1 0x79
#define G2 0x5B

#define N_STATES (1 << K)
#define MEM_DEPTH (N_STATES)
#define MAX_COST (MEM_DEPTH * 256)
#define BACKTRACK_DEPTH ((MEM_DEPTH-32) & 0xFFFFFFF8)
#define VITERBI_DELAY (MEM_DEPTH - BACKTRACK_DEPTH)

HardSource* viterbi_init(SoftSource *src);

int viterbi_encode(uint8_t *out, const uint8_t *in, size_t len);

#endif

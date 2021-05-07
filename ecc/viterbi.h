#ifndef viterbi_h
#define viterbi_h

#include <stdint.h>
#include "protocol/cadu.h"

#define G1 0x79             /* Connection polynomial #1 */
#define G2 0x5B             /* Connection polynomial #2 */
#define K 7                 /* Constraint length */
#define NUM_STATES (1<<K)   /* Number of possible encoder states */

#define MEM_DEPTH (NUM_STATES)              /* Maximum backtracking depth, in bits */
#define MEM_START (MEM_DEPTH * 1/2)         /* Portion of the backtracking to consider "not yet converged" */
#define MEM_BACKTRACE (MEM_DEPTH-MEM_START) /* Complementary of MEM_START */
#define VITERBI_DELAY (MEM_START/8)         /* Internal buffer size, in bytes */

#if (MEM_START % 8)
#error "MEM_START should be a multiple of 8"
#endif
#if ((1024 - VITERBI_DELAY) % (MEM_BACKTRACE >> 3)) \
	|| (VITERBI_DELAY % (MEM_BACKTRACE >> 3))
#error "Incompatible MEM_BACKTRACE size"
#endif

/**
 * Convolutionally encode a 32-bit word given a starting state. The connection
 * polynomials used will be G1 and G2, and the constraint length will be K as
 * #define'd above.
 *
 * @param output pointer to the memory region where the encoded data should be
 *        placed
 * @param state the starting internal state of the convolutional encoder
 * @param data the data to encode
 * @return the convolutional encoder's final state
 */
uint32_t conv_encode_u32(uint64_t *output, uint32_t state, uint32_t data);

/**
 * Initialize the Viterbi decoder
 */
void     viterbi_init();

/**
 * Decode soft symbols into bits using the Viterbi algorithm
 *
 * @param out pointer to the memory region where the decoded bytes should be
 *        written to
 * @param in pointer to the soft symbols to feed to the decoder
 * @param bytecount number of bytes to write to the output. Must be 1/16th the
 *        nmuber of valid soft symbols supplied to the decoder.
 * @return the total metric of the best path
 */
int     viterbi_decode(uint8_t *out, int8_t *in, int bytecount);

#endif

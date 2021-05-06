#ifndef utils_h
#define utils_h

#include <stdint.h>
#include <stdlib.h>

#define LEN(x) (sizeof(x)/sizeof(*x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define DO_PRAGMA(x) _Pragma(#x)

/* Portable unroll pragma, for some reason clang defines __GNUC__ but uses the
 * non-GCC unroll pragma format */
#if defined(__clang__)
#define PRAGMA_UNROLL(x) DO_PRAGMA(unroll x)
#elif defined(__GNUC__)
#define PRAGMA_UNROLL(x) DO_PRAGMA(GCC unroll x)
#else
#define PRAGMA_UNROLL(x) DO_PRAGMA(unroll x)
#endif

enum phase {
	PHASE_0=0, PHASE_90=1, PHASE_180=2, PHASE_270=3, // TODO might also need I<->Q swaps?
	PHASE_INV_0=4, PHASE_INV_90=5, PHASE_INV_180=6, PHASE_INV_270=7
};

/**
 * Convert soft samples into hard samples
 *
 * @param hard pointer to the destination buffer that will hold the hard
 *        samples. Must be at least len/8 bytes long
 * @param soft pointer to the soft samples to convert
 * @param len numebr of soft samples to convert
 */
void     soft_to_hard(uint8_t *hard, int8_t *soft, int len);

/**
 * Undo a rotation on a set of soft samples.
 *
 * @param soft the samples to rotate in-place
 * @param len number of samples to rotate
 * @param phase the phase rotation to undo
 */
void     soft_derotate(int8_t *soft, int len, enum phase phase);

/**
 * Count bits set inside of a variable
 *
 * @param v variable to count the bits of
 * @return number of bits set in the variable
 */
int      count_ones(uint64_t v);

/**
 * Read up to 32 misaligned bits from a byte buffer
 *
 * @param src pointer to the byte buffer to read bits from
 * @param offset_bits offset from the beginning of the pointer from which to
 *        start reading
 * @param bitcount number of bits to read
 * @return bits read
 */
uint32_t read_bits(const uint8_t *src, int offset_bits, int bitcount);

/**
 * Print usage information
 *
 * @param execname name of the executable
 */
void usage(char *execname);

/**
 * Print version information
 */
void version();

#endif

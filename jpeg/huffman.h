#ifndef huffman_h
#define huffman_h

#include <stdint.h>

/**
 * Decompress Huffman-compressed data into 8x8 blocks
 *
 * @param dst pointer to 8x8 blocks to write the decompressed data to
 * @param src pointer to the compressed data
 * @param count number of blocks to decompress
 * @param maxlen maximum number of compressed bytes available
 * @param dc the DC coefficient for the previous MCU (0 for he first one)
 * @return -1 if an error is encountered while decompressing
 *          0 on success
 */
int huffman_decode(int16_t (*dst)[8][8], const uint8_t *src, int count, int maxlen);

#endif

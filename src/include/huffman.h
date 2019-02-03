/**
 * Auxiliary functions to perform Huffman decoding operations on MCUs,
 * outputting the decompressed 8x8 thumbnails.
 */
#ifndef LRPTDEC_HUFFMAN_H
#define LRPTDEC_HUFFMAN_H

#include <stdint.h>
#include <stdlib.h>

void huffman_init();
int  huffman_decode(int16_t (*dst)[8][8], const uint8_t *src, size_t count);

#endif

#ifndef jpeg_h
#define jpeg_h
#include <stdint.h>

/**
 * Decode an 8x8 jpeg block into an 8x8 image.
 *
 * @param dst pointer to the destination 0x0 pixel array
 * @param src pointer to the encoded jpeg block
 * @param q the quality factor used when encoding the block
 */
void jpeg_decode(uint8_t dst[8][8], int16_t src[8][8], int q);


#endif /* jpeg_h */

/**
 * Auxiliary functions to help with decoding JPG thumbnails into raw pixels.
 */
#ifndef LRPTDEC_JPEG_H
#define LRPTDEC_JPEG_H

#include <stdint.h>
#include "packet.h"

void jpeg_init();
int  jpeg_decode(uint8_t dst[8][8], int16_t src[8][8], int quality);

#endif

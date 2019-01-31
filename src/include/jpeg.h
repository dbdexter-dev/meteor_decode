#ifndef LRPTDEC_JPEG_H
#define LRPTDEC_JPEG_H

#include <stdint.h>
#include "packet.h"

void jpeg_init();
int  jpeg_block_decode(uint8_t dst[8][8], const Mcu *mcu);

#endif

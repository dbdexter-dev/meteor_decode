#ifndef mcu_parser_h
#define mcu_parser_h

#include <stdint.h>
#include "protocol/mcu.h"

int avhrr_decode(uint8_t (*dst)[8][8], AVHRR *a, int len);

#endif

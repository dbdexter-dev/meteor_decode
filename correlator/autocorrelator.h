#ifndef autocorrelator_h
#define autocorrelator_h

#include <stdint.h>
#include "utils.h"

int autocorrelate(enum phase *rotation, int period, uint8_t *restrict hard, int len);

#endif

#ifndef LRPTDEC_CORRELATOR_H
#define LRPTDEC_CORRELATOR_H

#include <stdint.h>
#include <stdlib.h>
#include "source.h"

#define CORRELATION_THR 55

SoftSource* correlator_init_soft(SoftSource *src, uint8_t syncword[8]);
HardSource* correlator_init_hard(HardSource *src, uint8_t syncword[4]); /* TODO */

#endif

#ifndef LRPTDEC_CORRELATOR_H
#define LRPTDEC_CORRELATOR_H

#include <stdint.h>
#include <stdlib.h>

#define CORRELATION_THR 55

typedef struct {
	uint8_t (*patterns)[8];
	size_t pattern_count;
	int active_correction;
} Correlator;

Correlator* correlator_init(uint8_t syncword[8]);
int         correlator_soft_correlate(Correlator *c, const int8_t* frame, size_t len);
size_t      correlator_soft_errors(const int8_t* frame, const uint8_t* ref, size_t len);
void        correlator_soft_fix(Correlator *c, int8_t* frame, size_t len);
void        correlator_deinit(Correlator *c);

#endif

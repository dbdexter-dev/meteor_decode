/**
 * Miscellaneous self-contained functions
 */
#ifndef LRPTDEC_UTILS_H
#define LRPTDEC_UTILS_H

#define MAX(X, Y) ((X > Y) ? X : Y)
#define MIN(X, Y) ((X < Y) ? X : Y)

#include <stdlib.h>
#include <stdint.h>

typedef enum {
	PHASE_90,
	PHASE_180,
	PHASE_270,
} Phase;

void bit_expand(uint8_t *dst, const uint8_t *src, size_t len);
int  correlation(const uint8_t *x, const uint8_t *y, int len);
void iq_rotate_hard(uint8_t *buf, size_t count, Phase p);
void iq_rotate_soft(int8_t *buf, size_t count, Phase p);
void iq_reverse_hard(uint8_t *buf, size_t count);
void iq_reverse_soft(int8_t *buf, size_t count);

int   count_ones(uint8_t val);
void  fatal(char *msg);
char* gen_fname(int apid);
void  parse_apids(int *apid_list, char *raw);
void* safealloc(size_t size);
void  splash(void);
char* timeofday(unsigned int msec);
void  usage(const char *pname);
void  version(void);

#endif

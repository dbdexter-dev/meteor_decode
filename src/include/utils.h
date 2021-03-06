/**
 * Miscellaneous self-contained functions
 */
#ifndef LRPTDEC_UTILS_H
#define LRPTDEC_UTILS_H

#define MAX(X, Y) ((X > Y) ? X : Y)
#define MIN(X, Y) ((X < Y) ? X : Y)

#include <stdlib.h>
#include <stdint.h>

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

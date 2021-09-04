#ifndef bmp_out_h
#define bmp_out_h

#include "channel.h"

typedef struct _bmpout BmpOut;

int bmp_init(void **dst, const char *fname, int width, int height, int mono);
int bmp_write_rgb(void *bmp, Channel *red, Channel *green, Channel *blue);
int bmp_write_mono(void *bmp, Channel *ch);
int bmp_finalize(void *bmp);

#endif /* bmp_out_h */

#ifndef png_out_h
#define png_out_h

#include <stdint.h>
#include <png.h>
#include "channel.h"

typedef struct _pngout PngOut;

int png_init(void **png, const char *fname, int width, int height, int mono);
int png_write_rgb(void *png, Channel *red, Channel *green, Channel *blue);
int png_write_mono(void *png, Channel *ch);
int png_finalize(void *png);

#endif

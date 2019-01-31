#ifndef LRPTDEC_BMP_H
#define LRPTDEC_BMP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BMP_BPP 24
#define PX_PER_ROW 1568

typedef struct {
	FILE *fd;
	uint8_t strip[8][PX_PER_ROW];
	int cur_col;
	int num_rows;
} BmpSink;

BmpSink *bmp_open(const char *fname);
void     bmp_close(BmpSink *bmp);
int      bmp_append_block(BmpSink *bmp, uint8_t block[8][8]);

#endif

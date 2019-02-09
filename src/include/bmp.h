/**
 * Functions/structs that offer a simple interface to write BMP files, handling
 * header updates automatically 
 */
#ifndef LRPTDEC_BMP_H
#define LRPTDEC_BMP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BMP_BPP 24
#define PX_PER_ROW 1568

typedef enum {
	RED = 0,
	GREEN = 1,
	BLUE = 2,
	ALL = 3
} BmpChannel;

typedef struct {
	FILE *fd;
	uint8_t strip[8][PX_PER_ROW][3];
	int col_r, col_g, col_b;
	int num_rows;
} BmpSink;

BmpSink *bmp_open(const char *fname);
void     bmp_close(BmpSink *bmp);

int  bmp_append(BmpSink *bmp, uint8_t block[8][8], BmpChannel c);
void bmp_skip_lines(BmpSink *bmp, int count);

#endif

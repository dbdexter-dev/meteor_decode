#ifndef LRPTDEC_FILE_H
#define LRPTDEC_FILE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct src{
	FILE *fd;
	int bps;
} Source;

Source* src_open(const char *path, int bps);
int     src_read(Source *src, size_t count, int8_t *buf);
void    src_close(Source *src);


#endif

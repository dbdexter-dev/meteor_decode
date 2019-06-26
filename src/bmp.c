#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmp.h"
#include "utils.h"

typedef struct {
	uint8_t magicnum[2];
	uint32_t size;
	uint32_t reserved;
	uint32_t pixoffset;
}__attribute__((packed)) Bmpheader;

typedef struct {
	uint32_t dibsize;
	uint32_t width;
	uint32_t height;
	uint16_t num_colorplanes;
	uint16_t bits_per_px;
	uint32_t compression;
	uint32_t imgsize;
	uint32_t horiz_res;
	uint32_t vert_res;
	uint32_t num_colors;
	uint32_t num_important_colors;
}__attribute__((packed)) DIBheader;

static void update_header(BmpSink *bmp);
static void write_strip(BmpSink *bmp);

BmpSink*
bmp_open(const char *fname, unsigned imgwidth)
{
	Bmpheader hdr;
	DIBheader dib;
	FILE *fd;
	BmpSink *ret = NULL;

	hdr.magicnum[0] = 0x42;
	hdr.magicnum[0] = 0x4d;
	hdr.reserved = 0;
	hdr.size = sizeof(Bmpheader) + sizeof(DIBheader);
	hdr.pixoffset = sizeof(Bmpheader) + sizeof(DIBheader);

	dib.dibsize = sizeof(DIBheader);
	dib.width = imgwidth;
	dib.height = 0;
	dib.num_colorplanes = 1;
	dib.bits_per_px = BMP_BPP;
	dib.compression = 0;
	dib.imgsize = hdr.size;
	dib.horiz_res = 0;
	dib.vert_res = 0;
	dib.num_colors = 0;
	dib.num_important_colors = 0;

	if ((fd = fopen(fname, "w"))) {
		fwrite(&hdr, sizeof(hdr), 1, fd);
		fwrite(&dib, sizeof(dib), 1, fd);

		ret = safealloc(sizeof(*ret));

		ret->col_r = 0;
		ret->col_g = 0;
		ret->col_b = 0;
		ret->num_rows = 0;
		ret->fd = fd;
		ret->width = imgwidth;
		ret->strip = calloc(8*3, imgwidth);

		memset(ret->strip, 0, sizeof(*ret->strip) * 8 * imgwidth);
	} else {
		fatal("Could not open output file");
	}

	return ret;
}

/* Finalize the BMP and free the BmpSink */
void
bmp_close(BmpSink *bmp)
{
	write_strip(bmp);
	update_header(bmp);

	fclose(bmp->fd);
	free(bmp->strip);
	free(bmp);
}


/* Queue up a block for writing */
int
bmp_append(BmpSink *bmp, uint8_t block[8][8], BmpChannel c)
{
	int i, j;
	int ret;

	ret = 0;

	switch(c) {
	case RED:
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[(i*bmp->width+bmp->col_r+j)*3+2] = block[i][j];
			}
		}
		bmp->col_r += 8;
		ret = bmp->col_r;
		break;
	case GREEN:
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[(i*bmp->width+bmp->col_g+j)*3+1] = block[i][j];
			}
		}
		bmp->col_g += 8;
		ret = bmp->col_g;
		break;
	case BLUE:
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[(i*bmp->width+bmp->col_b+j)*3+0] = block[i][j];
			}
		}
		bmp->col_b += 8;
		ret = bmp->col_b;
		break;
	case ALL:       /* Fallthrough */
	default:
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[(i*bmp->width+bmp->col_r+j)*3+2] = block[i][j];
				bmp->strip[(i*bmp->width+bmp->col_g+j)*3+1] = block[i][j];
				bmp->strip[(i*bmp->width+bmp->col_b+j)*3+0] = block[i][j];
			}
		}
		bmp->col_r += 8;
		bmp->col_g += 8;
		bmp->col_b += 8;
		ret = bmp->col_r;
		break;
	}

	return ret;
}

void
bmp_newstrip(BmpSink *bmp)
{
	if (bmp->col_r || bmp->col_g || bmp->col_b) {
		write_strip(bmp);
		update_header(bmp);
	}
}

/* Static functions {{{ */
/* Update the BMP header */
static void
update_header(BmpSink *bmp)
{
	Bmpheader hdr;
	DIBheader dib;
	long cur_pos;

	dib.dibsize = sizeof(DIBheader);
	dib.width = bmp->width;
	dib.height = bmp->num_rows;
	dib.num_colorplanes = 1;
	dib.bits_per_px = BMP_BPP;
	dib.compression = 0;
	dib.imgsize = bmp->num_rows * dib.width;
	dib.horiz_res = 0;
	dib.vert_res = 0;
	dib.num_colors = 0;
	dib.num_important_colors = 0;

	hdr.magicnum[0] = 0x42;
	hdr.magicnum[1] = 0x4d;
	hdr.reserved = 0;
	hdr.size = sizeof(Bmpheader) + sizeof(DIBheader) + dib.imgsize;
	hdr.pixoffset = sizeof(Bmpheader) + sizeof(DIBheader);

	cur_pos = ftell(bmp->fd);

	fseek(bmp->fd, 0L, SEEK_SET);
	fwrite(&hdr, sizeof(hdr), 1, bmp->fd);
	fwrite(&dib, sizeof(dib), 1, bmp->fd);

	fseek(bmp->fd, cur_pos, SEEK_SET);
}

/* Write a queued strip one row at a time */
static void
write_strip(BmpSink *bmp)
{
	fwrite(bmp->strip, 8 * 3 * bmp->width, 1, bmp->fd);
	bmp->num_rows += 8;
	bmp->col_r = 0;
	bmp->col_g = 0;
	bmp->col_b = 0;
	memset(bmp->strip, 0, 8 * 3 * bmp->width);
}
/*}}}*/

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
bmp_open(const char *fname)
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
	dib.width = PX_PER_ROW;
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
		memset(ret->strip, 0, sizeof(ret->strip));
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
		if (bmp->col_r >= PX_PER_ROW) {
			write_strip(bmp);
			update_header(bmp);
		}
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[i][bmp->col_r+j][2] = block[i][j];
			}
		}
		bmp->col_r += 8;
		ret = bmp->col_r;
		break;
	case GREEN:
		if (bmp->col_g >= PX_PER_ROW) {
			write_strip(bmp);
			update_header(bmp);
		}
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[i][bmp->col_g+j][1] = block[i][j];
			}
		}
		bmp->col_g += 8;
		ret = bmp->col_g;
		break;
	case BLUE:
		if (bmp->col_b >= PX_PER_ROW) {
			write_strip(bmp);
			update_header(bmp);
		}
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[i][bmp->col_b+j][0] = block[i][j];
			}
		}
		bmp->col_b += 8;
		ret = bmp->col_b;
		break;
	case ALL:       /* Fallthrough */
	default:
		if (bmp->col_r >= PX_PER_ROW || bmp->col_g >= PX_PER_ROW || bmp->col_b >= PX_PER_ROW) {
			write_strip(bmp);
			update_header(bmp);
		}
		for (i=0; i<8; i++) {
			for (j=0; j<8; j++) {
				bmp->strip[i][bmp->col_r+j][0] = block[i][j];
				bmp->strip[i][bmp->col_g+j][1] = block[i][j];
				bmp->strip[i][bmp->col_b+j][2] = block[i][j];
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

/* Skip count lines by drawing them black */
void
bmp_skip_lines(BmpSink *bmp, int count)
{
	for (; count>0; count--) {
		write_strip(bmp);
	}
}

/* Static functions {{{ */
/* Update the BMP header. Note the negative height: this is because BMP stores
 * pixels left to right, bottom to top, and the image comes in top to bottom */
static void
update_header(BmpSink *bmp)
{
	Bmpheader hdr;
	DIBheader dib;
	long cur_pos;

	hdr.magicnum[0] = 0x42;
	hdr.magicnum[1] = 0x4d;
	hdr.reserved = 0;
	hdr.size = sizeof(Bmpheader) + sizeof(DIBheader) + bmp->num_rows * PX_PER_ROW;
	hdr.pixoffset = sizeof(Bmpheader) + sizeof(DIBheader);

	dib.dibsize = sizeof(DIBheader);
	dib.width = PX_PER_ROW;
	dib.height = bmp->num_rows;
	dib.num_colorplanes = 1;
	dib.bits_per_px = BMP_BPP;
	dib.compression = 0;
	dib.imgsize = bmp->num_rows * PX_PER_ROW;
	dib.horiz_res = 0;
	dib.vert_res = 0;
	dib.num_colors = 0;
	dib.num_important_colors = 0;

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
	fwrite(bmp->strip, sizeof(bmp->strip), 1, bmp->fd);
	bmp->num_rows += 8;
	bmp->col_r = 0;
	bmp->col_g = 0;
	bmp->col_b = 0;
	memset(bmp->strip, 0, sizeof(bmp->strip));
}
/*}}}*/

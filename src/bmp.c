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
		ret->cur_col = 0;
		ret->fd = fd;
		memset(ret->strip, 0, sizeof(ret->strip));
	}

	return ret;
}

void
bmp_close(BmpSink *bmp)
{
	if (bmp->cur_col > 0) {
		write_strip(bmp);
	}

	update_header(bmp);
	fclose(bmp->fd);
	free(bmp);
}

/* Queue up a block for writing */
int
bmp_append_block(BmpSink *bmp, uint8_t block[8][8])
{
	int i, j;

	for (i=0; i<8; i++) {
		for (j=0; j<8; j++) {
			bmp->strip[i][bmp->cur_col+j] = block[i][j];
		}
	}
	bmp->cur_col += 8;
	if (bmp->cur_col >= PX_PER_ROW) {
		write_strip(bmp);
		bmp->cur_col = 0;
	}
}

/* Static functions {{{ */

static void
update_header(BmpSink *bmp)
{
	Bmpheader hdr;
	DIBheader dib;
	long cur_pos;

	hdr.magicnum[0] = 0x42;
	hdr.magicnum[1] = 0x4d;
	hdr.size = sizeof(Bmpheader) + sizeof(DIBheader) + bmp->num_rows * PX_PER_ROW;
	hdr.pixoffset = sizeof(Bmpheader) + sizeof(DIBheader);

	dib.dibsize = sizeof(DIBheader);
	dib.width = PX_PER_ROW;
	dib.height = bmp->num_rows;
	dib.num_colorplanes = 1;
	dib.bits_per_px = 8 * 3;
	dib.compression = 0;
	dib.imgsize = bmp->num_rows * PX_PER_ROW;
	dib.horiz_res = 0;
	dib.vert_res = 0;
	dib.num_colors = 0;
	dib.num_important_colors = 0;

	cur_pos = ftell(bmp->fd);

	rewind(bmp->fd);
	fwrite(&hdr, sizeof(hdr), 1, bmp->fd);
	fwrite(&dib, sizeof(dib), 1, bmp->fd);

	fseek(bmp->fd, cur_pos, SEEK_SET);
}

/* Write a queued strip one row at a time */
static void
write_strip(BmpSink *bmp)
{
	int i, j;
	uint8_t px[3];

	for (i=0; i<8; i++) {
		for (j=0; j<PX_PER_ROW; j++) {
			px[0] = px[1] = px[2] = bmp->strip[i][j];
			fwrite(px, sizeof(px), 1, bmp->fd);
		}
	}

	bmp->num_rows += 8;
	memset(bmp->strip, 0, sizeof(bmp->strip));
}
/*}}}*/

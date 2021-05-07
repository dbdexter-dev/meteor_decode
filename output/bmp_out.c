#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "bmp_out.h"

struct _bmpout {
	FILE *fd;
	unsigned int width, height;
	uint8_t *rgbrow;
	int ready;
};

struct _bmphdr {
	/* Bitmap header */
	char header[2];
	uint32_t size;
	uint16_t reserved1, reserved2;
	uint32_t offset;

	/* DIB header, version BITMAPINFOHEADER */
	uint32_t dib_size;
	int32_t width;
	int32_t height;
	uint16_t color_planes;
	uint16_t bpp;
	uint32_t compression;
	uint32_t bitmap_size;
	int32_t horiz_resolution;
	int32_t vert_resolution;
	uint32_t color_count;
	uint32_t important_color_count;
}__attribute__((packed));

int
bmp_init(void **_dst, const char *fname, int width, int height, int mono)
{
	BmpOut **dst = (BmpOut**)_dst;
	const int dib_size = 40;
	BmpOut *bmp;
	struct _bmphdr header;
	int i;
	uint8_t color_entry[4];


	*dst = NULL;
	if (!(bmp = malloc(sizeof(*bmp)))) return 1;
	*dst = bmp;
	bmp->ready = 0;

	if (!(bmp->fd = fopen(fname, "wb"))) return 1;
	if (!(bmp->rgbrow = malloc(3*width))) return 1;

	bmp->width = width;
	bmp->height = height;

	header.header[0] = 'B'; header.header[1] = 'M';
	header.reserved1 = header.reserved2 = 0;
	header.compression = 0;
	header.color_planes = 1;
	header.color_count = 0;
	header.important_color_count = 0;
	header.horiz_resolution = header.vert_resolution = 0;

	header.dib_size = dib_size;
	header.bpp = mono ? 8 : 24;
	header.width = width;
	header.height = height;
	header.bitmap_size = width * height * header.bpp/8;
	header.size = header.bitmap_size;
	header.offset = sizeof(struct _bmphdr) + (mono ? 4*256: 0);

	/* Write BMP header */
	fwrite(&header, sizeof(header), 1, bmp->fd);

	/* If monochrome, BMP must use a color table: write that too */
	if (mono) {
		for (i=0; i<256; i++) {
			color_entry[0] = color_entry[1] = color_entry[2] = i;
			color_entry[3] = 0xFF;
			fwrite(&color_entry, 4, 1, bmp->fd);
		}
	}

	bmp->ready = 1;
	return 0;
}

int
bmp_write_rgb(void *_bmp, Channel *red, Channel *green, Channel *blue)
{
	BmpOut *bmp = (BmpOut*)_bmp;
	unsigned int i, col;
	uint8_t *rgbrow;
	int offset;

	if (!bmp || !bmp->ready) return 1;

	offset = (bmp->height-1)*bmp->width;

	for (i=0; i<bmp->height; i++) {
		rgbrow = bmp->rgbrow;

		for (col=0; col<bmp->width; col++) {
			*rgbrow++ = (offset < blue->offset) ? blue->pixels[offset] : 0;
			*rgbrow++ = (offset < green->offset) ? green->pixels[offset] : 0;
			*rgbrow++ = (offset < red->offset) ? red->pixels[offset] : 0;
			offset++;
		}

		fwrite(bmp->rgbrow, 3*bmp->width, 1, bmp->fd);
		offset -= 2*bmp->width;
	}

	return 0;
}

int
bmp_write_mono(void *_bmp, Channel *ch)
{
	BmpOut *bmp = (BmpOut*)_bmp;
	int i;

	if (!bmp || !bmp->ready) return 1;

	for (i=bmp->height-1; i>=0; i--) {
		fwrite(ch->pixels + bmp->width*i, bmp->width, 1, bmp->fd);
	}

	return 0;
}

int
bmp_finalize(void *_bmp)
{
	BmpOut *bmp = _bmp;

	if (!bmp || !bmp->ready) return 1;

	fclose(bmp->fd);
	free(bmp->rgbrow);
	free(bmp);

	return 0;
}

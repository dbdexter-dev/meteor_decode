#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "png_out.h"
#include "protocol/mpdu.h"

struct _pngout{
	FILE *fd;
	unsigned int width, height;
	uint8_t *rgbrow;
	png_structp png_ptr;
	png_infop   info_ptr;
	int ready;
};

int
png_init(void **_dst, const char *fname, int width, int height, int mono)
{
	PngOut **dst = (PngOut**)_dst;
	PngOut *png;

	*dst = NULL;
	if(!(png = malloc(sizeof(*png)))) return 1;
	*dst = png;
	png->ready = 0;

	png->width = width;
	png->height = height;

	if (!(png->fd = fopen(fname, "wb"))) return 1;
	if (!(png->rgbrow = malloc(3*width))) return 1;

	png->png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png->png_ptr) return 1;

	png->info_ptr = png_create_info_struct(png->png_ptr);
	if (!png->info_ptr) return 1;

	if (setjmp(png_jmpbuf(png->png_ptr))) {
		png_destroy_write_struct(&png->png_ptr, &png->info_ptr);
		free(png->rgbrow);
		fclose(png->fd);
		return 2;
	}

	png_init_io(png->png_ptr, png->fd);
	png_set_IHDR(png->png_ptr, png->info_ptr, png->width, png->height, 8,
	             (mono ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB),
	             PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png->png_ptr, png->info_ptr);

	png->ready = 1;
	return 0;
}

int
png_write_rgb(void *_png, Channel *red, Channel *green, Channel *blue)
{
	PngOut *png = _png;
	unsigned int i, col;
	unsigned long r, g, b;
	png_structp png_ptr = (png_structp)png->png_ptr;
	png_infop info_ptr = (png_infop)png->info_ptr;
	uint8_t *rgbrow;

	if (!png || !png->ready) return 1;

	if (setjmp(png_jmpbuf(png->png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		png->png_ptr = NULL;
		png->info_ptr = NULL;
		return 1;
	}

	r = g = b = 0;

	/* Interleave the three color channels */
	for (i=0; i<png->height; i++) {
		rgbrow = png->rgbrow;

		for (col=0; col<png->width; col++) {
			*rgbrow++ = (r < red->offset) ? red->pixels[r++] : 0;
			*rgbrow++ = (g < green->offset) ? green->pixels[g++] : 0;
			*rgbrow++ = (b < blue->offset) ? blue->pixels[b++] : 0;
		}

		png_write_row(png_ptr, png->rgbrow);
	}

	return 0;
}

int
png_write_mono(void *_png, Channel *ch)
{
	PngOut *png = _png;
	png_structp png_ptr = (png_structp)png->png_ptr;
	png_infop info_ptr = (png_infop)png->info_ptr;
	unsigned int i;

	if (!png || !png->ready) return 1;

	if (setjmp(png_jmpbuf(png->png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		png->png_ptr = NULL;
		png->info_ptr = NULL;
		return 1;
	}

	for (i=0; i<png->height; i++) {
		png_write_row(png_ptr, ch->pixels + (png->width*i));
	}
	return 0;
}

int
png_finalize(void *_png)
{
	PngOut *png = _png;

	if (!png || !png->ready) return 1;

	png_write_end(png->png_ptr, png->info_ptr);
	png_free_data(png->png_ptr, png->info_ptr, PNG_FREE_ALL, -1);
	png_destroy_info_struct(png->png_ptr, &png->info_ptr);
	png_destroy_write_struct(&png->png_ptr, NULL);
	fclose(png->fd);
	free(png->rgbrow);
	free(png);

	return 0;
}

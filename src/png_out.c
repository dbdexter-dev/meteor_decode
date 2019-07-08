#include <png.h>
#include <stdio.h>
#include "channel.h"
#include "packet.h"
#include "png_out.h"
#include "utils.h"

#define WIDTH (MCU_PER_PP * 8)

void
png_compose(FILE *fd, Channel *red, Channel *green, Channel *blue)
{
	int row, col, block_num, block_offset;
	size_t pixel_idx;
	png_byte red_px, green_px, blue_px;
	int height;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep row_ptr;

	if (!fd) {
		fatal("Output fd is null");
	}

	/* Compute the image height: if it is zero, just return */
	height = MAX(red->len, MAX(green->len, blue->len)) / WIDTH;
	if (height == 0) {
		return;
	}

	/* Initialize all the png related stuff */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fatal("Could not allocate png write struct");
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		fatal("Could not allocate png info struct");
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		fatal("Error during the creation of the png");
	}

	png_init_io(png_ptr, fd);
	if (setjmp(png_jmpbuf(png_ptr))) {
		fatal("Error during the creation of the png");
	}

	png_set_IHDR(png_ptr, info_ptr, WIDTH, height, 8, PNG_COLOR_TYPE_RGB,
	             PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
	             PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info_ptr);

	row_ptr = (png_bytep) safealloc(3 * WIDTH * sizeof(png_byte));

	/* Write the image  line by line */
	for (row=0; row<height; row++) {
		for (col=0; col<WIDTH; col++) {
			block_num = col / 8;
			block_offset = col % 8;

			/* Channel buffer structure is not linear, find the offset
			 * corresponding to the required row/column coordinate */
			pixel_idx = (64*block_num + block_offset) + (((row % 8) + WIDTH * (row/8)) * 8);

			/* Some channels may be shorter than others. In that case, just
			 * write black pixels to that channel */
			red_px  = (pixel_idx >= red->len ? 0 : red->data[pixel_idx]);
			green_px  = (pixel_idx >= green->len ? 0 : green->data[pixel_idx]);
			blue_px  = (pixel_idx >= blue->len ? 0 : blue->data[pixel_idx]);

			row_ptr[col*3+0] = red_px;
			row_ptr[col*3+1] = green_px;
			row_ptr[col*3+2] = blue_px;
		}
		png_write_row(png_ptr, row_ptr);
	}

	/* Finalize the PNG and free the used buffer */
	png_write_end(png_ptr, info_ptr);
	png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	png_destroy_write_struct(&png_ptr, NULL);
	free(row_ptr);
}


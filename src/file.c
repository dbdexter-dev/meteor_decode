#include <stdio.h>
#include "file.h"
#include "utils.h"

Source*
src_open(const char *path, int bps) {
	Source *ret;

	ret = safealloc(sizeof(*ret));
	ret->bps = (bps <= 0 ? 8 : bps);

	if(!(ret->fd = fopen(path, "r"))) {
		fatal("Could not find specified file");
		/* Not reached */
		return NULL;
	}

	return ret;
}

/* Read samples and normalize them to 8-bit values */
int
src_read(Source *src, size_t count, int8_t *buf) {
	int samples_read;

	if (src->bps == 8) {
		samples_read = fread(buf, sizeof(*buf), count, src->fd);
	} else {
		fatal("Not yet implemented");
	}

	return samples_read;
}

void
src_close(Source *src) {
	if(src) {
		fclose(src->fd);
		free(src);
	}
}


#include <stdio.h>
#include "file.h"
#include "source.h"
#include "utils.h"

typedef struct {
	FILE *fd;
	int bps;
} FileSrc;

static int src_soft_read(SoftSource *src, int8_t *buf, size_t count);
static int src_soft_close(SoftSource *src);

SoftSource*
src_soft_open(const char *path, int bps) {
	SoftSource *ret;
	FileSrc *backend;

	ret = safealloc(sizeof(*ret));
	backend = safealloc(sizeof(*backend));

	ret->close = src_soft_close;
	ret->read = src_soft_read;
	ret->_backend = backend;

	if (bps == 0) {
		bps = 8;
	}

	backend->bps = bps;
	if(!(backend->fd = fopen(path, "r"))) {
		fatal("Could not find specified file");
		/* Not reached */
		return NULL;
	}

	return ret;
}

/* Read samples and normalize them to 8-bit values */
int
src_soft_read(SoftSource *self, int8_t *buf, size_t count) {
	int samples_read;
	FileSrc *src = self->_backend;

	if (!buf) {
		fseek(src->fd, count, SEEK_CUR);
		return 0;
	}

	samples_read = 0;
	if (src->bps == 8) {
		samples_read = fread(buf, sizeof(*buf), count, src->fd);
	} else {
		fatal("Not yet implemented");
	}

	return samples_read;
}

int
src_soft_close(SoftSource *self) {
	FileSrc *src = self->_backend;

	if(src) {
		fclose(src->fd);
		free(src);
	}

	free(self);
	return 0;
}


/**
 * Opaque data structures useful to abstract sample/bit sources from their
 * origin */
#ifndef LRPTDEC_SOURCE_H
#define LRPTDEC_SOURCE_H

#include <stdlib.h>
#include <stdint.h>

typedef struct softsource {
	int (*read)(struct softsource *, int8_t *, size_t);
	int (*close)(struct softsource *);
	void *_backend;
} SoftSource;

typedef struct hardsource {
	int (*read)(struct hardsource *, uint8_t *, size_t);
	int (*close)(struct hardsource *);
	void *_backend;
} HardSource;

#endif

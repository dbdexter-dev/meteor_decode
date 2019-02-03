/**
 * Opaque data structures useful for abstracting sample/bit sources from their
 * origin 
 */
#ifndef LRPTDEC_SOURCE_H
#define LRPTDEC_SOURCE_H

#include <stdlib.h>
#include <stdint.h>

/* Generic source of soft samples (somewhat """analog""") */
typedef struct softsource {
	int (*read)(struct softsource *, int8_t *out, size_t len);
	int (*close)(struct softsource *);
	void *_backend;
} SoftSource;

/* Generic bitstream source */
typedef struct hardsource {
	int (*read)(struct hardsource *, uint8_t *out, size_t len);
	int (*close)(struct hardsource *);
	void *_backend;
} HardSource;

#endif

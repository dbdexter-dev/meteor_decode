#ifndef LRPTDEC_PNG_H
#define LRPTDEC_PNG_H

#include <stdio.h>
#include "channel.h"

void png_compose(FILE *fd, Channel *red, Channel *green, Channel *blue);

#endif

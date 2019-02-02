#ifndef LRPTDEC_FILE_H
#define LRPTDEC_FILE_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "source.h"

SoftSource* src_soft_open(const char *path, int bps);
HardSource* src_hard_open(const char *path); /* TODO */

#endif

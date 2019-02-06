#ifndef LRPT_OPTIONS_H
#define LRPT_OPTIONS_H

#include <getopt.h>
#include <stdlib.h>

#define SHORTOPTS "a:ho:v"
struct option longopts[] = {
	{ "apid",       1, NULL, 'a' },
	{ "help",       0, NULL, 'h' },
	{ "output",     1, NULL, 'o' },
	{ "version",    0, NULL, 'v' },
};

#endif

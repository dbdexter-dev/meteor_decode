#ifndef LRPT_OPTIONS_H
#define LRPT_OPTIONS_H

#include <getopt.h>
#include <stdlib.h>

#define SHORTOPTS "a:dho:qsSv"
struct option longopts[] = {
	{ "apid",       1, NULL, 'a' },
	{ "diff",       0, NULL, 'd' },
	{ "help",       0, NULL, 'h' },
	{ "output",     1, NULL, 'o' },
	{ "quiet",      0, NULL, 'q' },
	{ "statfile",   0, NULL, 's' },
	{ "split",      0, NULL, 'S' },
	{ "version",    0, NULL, 'v' },
};

#endif

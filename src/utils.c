#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utils.h"

static char _time_of_day[sizeof("HH:MM:SS.mmm")];


/* Count the ones in a uint8_t */
int
count_ones(uint8_t val)
{
	int i, ret;

	ret = 0;
	for (i=0; i<8; i++) {
		ret += (val >> i) & 0x01;
	}
	return ret;
}

void
fatal(char *msg)
{
	fprintf(stderr, "[FATAL] %s\n", msg);
	exit(-1);
}

char*
gen_fname(int apid)
{
	char *ret, *tmp;
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);

	tmp = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM"));
	strftime(tmp, sizeof("LRPT_YYYY_MM_DD_HH-MM"),
			 "LRPT_%Y_%m_%d-%H_%M", tm);

	if (apid < 0) {
		ret = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM.png"));
		sprintf(ret, "%s.png", tmp);
	} else {
		ret = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM-AA.png"));
		sprintf(ret, "%s-%02d.png", tmp, apid);
	}

	free(tmp);

	return ret;
}

/* Parse the APID list */
void
parse_apids(int *apid_list, char *raw)
{
	sscanf(raw, "%d,%d,%d", apid_list, apid_list+1, apid_list+2);
}

/* Malloc with abort on error */
void*
safealloc(size_t size)
{
	void *ptr;
	ptr = malloc(size);
	if (!ptr) {
		fatal("Failed to allocate block");
	}

	return ptr;
}

void
splash()
{
	fprintf(stderr, "\nLRPT decoder v%s\n\n", VERSION);
}

char*
timeofday(unsigned int msec)
{
	int hr, min, sec, ms;

	ms =  msec % 1000;
	sec = msec / 1000 % 60;
	min = msec / 1000 / 60 % 60;
	hr =  msec / 1000 / 60 / 60 % 24;

	sprintf(_time_of_day, "%02d:%02d:%02d.%03d", hr, min, sec, ms);

	return _time_of_day;
}


void
usage(const char *pname)
{
	splash();
	fprintf(stderr, "Usage: %s [options] file_in\n", pname);
	fprintf(stderr,
			"   -a, --apid R,G,B        Specify APIDs to parse (default: 68,65,64)\n"
			"   -d, --diff              Differentially decode (e.g. for Metebr-M N2-2)\n"
			"   -o, --output <file>     Output composite png to <file>\n"
			"   -t, --statfile          Write the .stat file containing timing data\n"
			"\n"
			"   -h, --help              Print this help screen\n"
			"   -v, --version           Print version info\n"
		   );
	exit(0);
}

void
version()
{
	fprintf(stderr, "LRPT decoder v%s\n", VERSION);
	fprintf(stderr, "Released under the GNU GPLv3\n\n");
	exit(0);
}

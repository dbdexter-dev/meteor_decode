#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "utils.h"

static char _time_of_day[sizeof("HH:MM:SS.mmm")];

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
		ret = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM.bmp"));
		sprintf(ret, "%s.bmp", tmp);
	} else {
		ret = safealloc(sizeof("LRPT_YYYY_MM_DD-HH_MM-AA.bmp"));
		sprintf(ret, "%s-%02d.bmp", tmp, apid);
	}

	free(tmp);

	return ret;
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
			"   -o, --output <file>     Output composite bmp to <file>\n"
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

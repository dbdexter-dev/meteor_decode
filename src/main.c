#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bmp.h"
#include "decoder.h"
#include "file.h"
#include "options.h"
#include "utils.h"

int
main(int argc, char *argv[])
{
	int c;
	SoftSource *softsamples;
	BmpSink *bmp;
	Decoder *decoder;
	struct timespec timespec;

	/* Command-line changeable variables {{{*/
	int apid_list[3];
	char *out_fname, *in_fname;
	int free_fname_on_exit;
	int upd_interval;
	/*}}}*/
	/* Initialize command-line overridable parameters {{{*/
	out_fname = NULL;
	free_fname_on_exit = 0;
	apid_list[0] = 68;
	apid_list[1] = 65;
	apid_list[2] = 64;
	upd_interval = 50;
	/*}}}*/
	/* Parse command line args {{{ */
	if (argc < 2) {
		usage(argv[0]);
	}
	optind = 0;
	while ((c = getopt_long(argc, argv, SHORTOPTS, longopts, NULL)) != -1) {
		switch(c) {
		case 'a':
			parse_apids(apid_list, optarg);
			break;
		case 'h':
			usage(argv[0]);
			break;
		case 'o':
			out_fname = optarg;
			break;
		case 'v':
			version();
			break;
		}
	}

	/* Check that an input filename was given */
	if (argc - optind < 1) {
		usage(argv[0]);
	}

	in_fname = argv[optind];

	if (!out_fname) {
		out_fname = gen_fname(-1);
		free_fname_on_exit = 1;
	}
	/*}}}*/


	/* Let the dance begin! */
	softsamples = src_soft_open(in_fname, 8);
	bmp = bmp_open(out_fname);

	decoder = decoder_init(bmp, softsamples, apid_list);
	decoder_start(decoder);

	timespec.tv_sec = upd_interval/1000;
	timespec.tv_nsec = ((upd_interval - timespec.tv_sec*1000))*1000L*1000;

	splash();

	while (decoder_get_status(decoder)) {
		printf("0x%08X\t rs=%2d, APID %d, onboard time: %s\r",
		       decoder_get_syncword(decoder),
		       decoder_get_rs_count(decoder),
		       decoder_get_apid(decoder),
		       timeofday(decoder_get_time(decoder))
		       );
		fflush(stdout);
		nanosleep(&timespec, NULL);
	}
	printf("\n");

	bmp_close(bmp);
	softsamples->close(softsamples);

	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

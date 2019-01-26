#include <stdarg.h>
#include <stdio.h>
#include "correlator.h"
#include "lrpt.h"
#include "options.h"
#include "utils.h"
#include "viterbi.h"

const uint8_t SYNC_WORD[] = {0x1a, 0xcf, 0xfc, 0x1d};

int
main(int argc, char *argv[])
{
	int c;
	Source *s;
	Viterbi *v;
	Correlator *co;
	int8_t buf[SOFT_FRAME_SIZE];
	uint8_t decoded[512];
	int bytes_read;
	/* Command-line changeable variables {{{*/
	char *out_fname, *in_fname;
	int free_fname_on_exit;
	int (*log) (const char *msg, ...);
	/*}}}*/

	/* Initialize command-line overridable parameters {{{*/
	out_fname = NULL;
	free_fname_on_exit = 0;
	log = printf;
	/*}}}*/
	/* Parse command line args {{{ */
	if (argc < 2) {
		usage(argv[0]);
	}
	optind = 0;
	while ((c = getopt_long(argc, argv, SHORTOPTS, longopts, NULL)) != -1) {
		switch(c) {
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
	/*}}}*/

	if (!out_fname) {
		out_fname = gen_fname(-1);
		free_fname_on_exit = 1;
	}

	s = src_open(in_fname, 8);
	v = viterbi_init();

	co = correlator_init(SYNC_WORD);

	int i;
	for (i=0; i<100; i++) {
		src_read(s, SOFT_FRAME_SIZE, buf);
		correlator_soft_correlate(co, buf, SOFT_FRAME_SIZE);
/*		correlator_soft_fix(co, buf, SOFT_FRAME_SIZE);*/
/*		bytes_read = viterbi_decode(v, buf, SOFT_FRAME_SIZE, decoded);*/
/*		printf("Decoded %d bytes\n", bytes_read);*/
/*		fwrite(decoded, bytes_read, 1, stdout);*/
		fflush(stdout);
	}

	src_close(s);
	viterbi_deinit(v);
	correlator_deinit(co);


	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "correlator.h"
#include "options.h"
#include "pkt_processor.h"
#include "reedsolomon.h"
#include "file.h"
#include "utils.h"
#include "viterbi.h"


int
main(int argc, char *argv[])
{
	int c;
	int pkt_count, pkt_decoded;
	SoftSource *softsamples, *correlator;
	HardSource *viterbi;
	PktProcessor *pp;
	uint8_t encoded_syncword[2*sizeof(SYNCWORD)];
	Cadu cadu;
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

	pp = pkt_init();

	/* Let the chain begin! */
	softsamples = src_soft_open(in_fname, 8);
	viterbi_encode(encoded_syncword, SYNCWORD, sizeof(SYNCWORD));
	correlator = correlator_init_soft(softsamples, encoded_syncword);
	viterbi = viterbi_init(correlator);

	pkt_count = 0;
	pkt_decoded = 0;
	while(viterbi->read(viterbi, (uint8_t*)&cadu, sizeof(Cadu))){
		log("Sync: 0x%08X\t", cadu.sync_marker);
		if(pkt_process(pp, &cadu.cvcdu) >= 0) {
			pkt_decoded++;
		}
		printf("\n");

/*			fwrite((uint8_t*)&cadu, sizeof(cadu), 1, stdout);*/
			fflush(stdout);
	}
	printf("Recovered %d/%d packets\n", pkt_decoded, pkt_count);

	pkt_deinit(pp);
	/* Closing a link closes all the "sub-contractors" */
	viterbi->close(viterbi);


	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

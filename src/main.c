#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bmp.h"
#include "correlator.h"
#include "file.h"
#include "options.h"
#include "packetizer.h"
#include "reedsolomon.h"
#include "utils.h"
#include "viterbi.h"

int
main(int argc, char *argv[])
{
	int c;
	SoftSource *softsamples, *correlator;
	HardSource *viterbi;
	Segment seg;
	PktProcessor *pp;
	uint8_t encoded_syncword[2*sizeof(SYNCWORD)];
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


	/* Let the chain begin! */
/*	softsamples = src_soft_open(in_fname, 8);*/
/*	viterbi_encode(encoded_syncword, SYNCWORD, sizeof(SYNCWORD));*/
/*	correlator = correlator_init_soft(softsamples, encoded_syncword);*/
/*	viterbi = viterbi_init(correlator);*/
/*	pp = pkt_init(viterbi);*/

	uint8_t whiteblk[8][8];
	for (int i=0; i<8; i++)
		for (int j=0; j<8; j++)
			whiteblk[i][j] = 0x80;

	BmpSink *bmp = bmp_open("/tmp/test.bmp");
	bmp_append_block(bmp, whiteblk);
	bmp_close(bmp);
	


/*	while(pkt_read(pp, &seg)){*/
/*		if (seg.len > 0) {*/
/*			log("seq=%d len=%d APID=%d\n", seg.seq, seg.len, seg.apid);*/
/*			hexdump("Data", seg.data, seg.len);*/
/*		}*/
/*	}*/

	/* Closing a link closes all the "sub-contractors" */
/*	pkt_deinit(pp);*/

	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

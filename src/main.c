#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bmp.h"
#include "compositor.h"
#include "correlator.h"
#include "file.h"
#include "hexdump.h"
#include "huffman.h"
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
	Packetizer *pp;
	BmpSink *bmp;
	Compositor *comp;
	Segment seg;
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


	/* Let the dance begin! */
	softsamples = src_soft_open(in_fname, 8);
	viterbi_encode(encoded_syncword, SYNCWORD, sizeof(SYNCWORD));
	correlator = correlator_init_soft(softsamples, encoded_syncword);
	viterbi = viterbi_init(correlator);
	pp = pkt_init(viterbi);

	bmp = bmp_open("/tmp/test.bmp");
	comp = comp_init(bmp);

	huffman_init();
	while(pkt_read(pp, &seg)){
		if (seg.len > 0) {
			log("seq=%d len=%d APID=%d\n", seg.seq, seg.len, seg.apid);
/*			hexdump("Data", seg.data, seg.len);*/
			if (seg.apid == 64) {
				comp_compose(comp, &seg);
			}
		}
	}

	bmp_close(bmp);

	comp_deinit(comp);
	pkt_deinit(pp);
	viterbi->close(viterbi);
	correlator->close(correlator);
	softsamples->close(softsamples);

	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

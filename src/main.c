#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "correlator.h"
#include "options.h"
#include "packet.h"
#include "reedsolomon.h"
#include "utils.h"
#include "viterbi.h"


int
main(int argc, char *argv[])
{
	int c;
	Source *s;
	Viterbi *v;
	Correlator *co;
	ReedSolomon *rs;
	int8_t buf[SOFT_FRAME_SIZE];
	uint8_t decoded[FRAME_SIZE * 2];
	uint8_t encoded_syncword[2*sizeof(SYNCWORD)];
	int decoded_idx = 0;
	Cadu cadu;
	int bytes_read;
	int rs_fix_count;
	int frame_offset;
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


	rs = rs_init(INTERLEAVING);

	s = src_open(in_fname, 8);
	v = viterbi_init();
	viterbi_encode(encoded_syncword, SYNCWORD, sizeof(SYNCWORD));
	co = correlator_init(encoded_syncword);
	packet_init();

	for (int i=0; i<1000; i++) {
		/* Read a frame */
		src_read(s, SOFT_FRAME_SIZE, buf);
		/* Correlate to find the frame offset */
		frame_offset = correlator_soft_correlate(co, buf, SOFT_FRAME_SIZE);

		/* Correct for the frame offset */
		memmove(buf, buf+frame_offset, SOFT_FRAME_SIZE - frame_offset);
		src_read(s, frame_offset, buf + SOFT_FRAME_SIZE - frame_offset);

		/* Fix phase ambiguity */
		correlator_soft_fix(co, buf, SOFT_FRAME_SIZE);

		/* Feed frame to the Viterbi decoder */
		bytes_read = viterbi_decode(v, decoded+decoded_idx, buf, SOFT_FRAME_SIZE);
		decoded_idx += bytes_read;

		/* This is all because of the delay the Viterbi decoder introduces...
		 * thanks Viterbi, thanks a lot */
		if (decoded_idx >= (int)FRAME_SIZE - 1) {
			memcpy(&cadu, decoded, sizeof(cadu));
			memmove(decoded, decoded+sizeof(Cadu), decoded_idx - sizeof(Cadu));
			decoded_idx -= sizeof(Cadu);

			/* Descramble */
			packet_descramble(&cadu.cvcdu);
			/* Reed-Solomon fix */
			rs_fix_count = rs_fix_packet(rs, &cadu.cvcdu, NULL);

			if (rs_fix_count >= 0) {
				printf("Sync: 0x%08X\n", cadu.sync_marker);
				packet_vcdu_dump(&cadu.cvcdu);
				printf("\n");
			}

/*			fwrite((uint8_t*)&cadu, sizeof(cadu), 1, stdout);*/
			fflush(stdout);
		}
	}

	rs_deinit(rs);
	viterbi_deinit(v);
	correlator_deinit(co);
	src_close(s);


	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

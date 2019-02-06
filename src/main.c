#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bmp.h"
#include "correlator.h"
#include "file.h"
#include "huffman.h"
#include "options.h"
#include "packetizer.h"
#include "pixels.h"
#include "reedsolomon.h"
#include "utils.h"
#include "viterbi.h"

int
main(int argc, char *argv[])
{
	int i, c, channel, seq, seq_delta;
	int mcu_nr, align_okay;
	SoftSource *softsamples, *correlator;
	HardSource *viterbi;
	Packetizer *pp;
	BmpSink *bmp;
	PixelGen *pixelgen[3];
	Mcu *mcu;
	Segment seg;
	uint8_t encoded_syncword[2*sizeof(SYNCWORD)];

	/* Command-line changeable variables {{{*/
	int apid_list[3];
	char *out_fname, *in_fname;
	int free_fname_on_exit;
	int (*log) (const char *msg, ...);
	/*}}}*/
	/* Initialize command-line overridable parameters {{{*/
	out_fname = NULL;
	free_fname_on_exit = 0;
	log = printf;
	apid_list[0] = 68;
	apid_list[1] = 65;
	apid_list[2] = 64;
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
	viterbi_encode(encoded_syncword, SYNCWORD, sizeof(SYNCWORD));
	correlator = correlator_init_soft(softsamples, encoded_syncword);
	viterbi = viterbi_init(correlator);
	pp = pkt_init(viterbi);

	bmp = bmp_open(out_fname);
	pixelgen[0] = pixelgen_init(bmp, RED);
	pixelgen[1] = pixelgen_init(bmp, GREEN);
	pixelgen[2] = pixelgen_init(bmp, BLUE);

	seq = -1;

	while(pkt_read(pp, &seg)){
		if (seg.len > 0 && seg.apid) {
			log("\nseq=%5d len=%3d APID=%d, tstamp=%s", seg.seq, seg.len,
			    seg.apid, timeofday(seg.timestamp));

			mcu = (Mcu*)seg.data;
			mcu_nr = mcu_seq(mcu);
			seq_delta = (seg.seq - seq + 16384) % 16384;

			/* Compensate for lost MCUs */
			if (seq > 0 && seq_delta > 1) {
				/* Realign to the beginning if we lost the end of the current
				 * line, and add black lines to fill completely lost VCDUs.
				 * Note that the sequence number is not monotonic; it wraps
				 * around after reaching 16833. So seg.seq might be less than
				 * pkt_end if the 16384 threshold was reached */
				align_okay = 0;
				while (!align_okay) {
					for (channel=0; channel<3; channel++) {
						if (pixelgen[channel]->pkt_end < seg.seq || pixelgen[channel]->pkt_end - seg.seq > 8192) {
							printf("\nAligning %d to %d", pixelgen[channel]->pkt_end, seg.seq);
							for (i=pixelgen[channel]->mcu_nr; i<MCU_PER_PP; i += MCU_PER_MPDU) {
								pixelgen_append(pixelgen[channel], NULL);
							}
							pixelgen[channel]->mcu_nr = 0;
							pixelgen[channel]->pkt_end += 3*MPDU_PER_LINE+1;
						} else {
							align_okay = 1;
						}
					}
				}
			}

			for (channel=0; channel<3; channel++) {
				if (seg.apid == apid_list[channel]) {
					for (i = pixelgen[channel]->mcu_nr; i < mcu_nr; i += MCU_PER_MPDU) {
						pixelgen_append(pixelgen[channel], NULL);
					}
					pixelgen_append(pixelgen[channel], &seg);
				}
			}

			seq = seg.seq;
		} else {
			log(".");
			fflush(stdout);
		}
	}

	for (channel=0; channel<3; channel++) {
		pixelgen_deinit(pixelgen[channel]);
	}
	bmp_close(bmp);

	pkt_deinit(pp);
	viterbi->close(viterbi);
	correlator->close(correlator);
	softsamples->close(softsamples);

	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

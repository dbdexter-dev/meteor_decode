#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "bmp.h"
#include "correlator.h"
#include "file.h"
#include "options.h"
#include "packetizer.h"
#include "pixels.h"
#include "reedsolomon.h"
#include "utils.h"
#include "viterbi.h"

int
main(int argc, char *argv[])
{
	int i, c, ch;
	int total_count, valid_count, mcu_nr, seq_delta, last_seq;
	uint32_t last_tstamp;
	int align_okay, skip_zero;
	SoftSource *src, *correlator;
	HardSource *viterbi;
	Packetizer *pp;
	PixelGen *pxgen[3], *cur_gen;
	BmpSink *bmp;
	Segment seg;
	Mcu *mcu;
	uint8_t encoded_syncword[2*sizeof(SYNCWORD)];

	/* Command-line changeable variables {{{*/
	int apid_list[3];
	char *out_fname, *in_fname;
	int free_fname_on_exit;
	/*}}}*/
	/* Initialize command-line overridable parameters {{{*/
	out_fname = NULL;
	free_fname_on_exit = 0;
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

	splash();

	src = src_soft_open(in_fname, 8);
	viterbi_encode(encoded_syncword, SYNCWORD, sizeof(SYNCWORD));
	correlator = correlator_init_soft(src, encoded_syncword);
	viterbi = viterbi_init(correlator);
	pp = pkt_init(viterbi);
	bmp = bmp_open(out_fname, PX_PER_ROW);

	pxgen[0] = pixelgen_init(bmp, RED);
	pxgen[1] = pixelgen_init(bmp, GREEN);
	pxgen[2] = pixelgen_init(bmp, BLUE);

	last_seq = -1;
	last_tstamp = 0;
	total_count = 0;
	valid_count = 0;

	while (pkt_read(pp, &seg)) {
		total_count++;

		/* Skip invalid packets */
		if (seg.len <= 0) {
			continue;
		}
		valid_count++;

		printf("\nseq %5d, APID %d %s",
		       seg.seq,
		       seg.apid,
		       timeofday(seg.timestamp));

		if (seg.apid == 0) {
			/* Meteor-M2 has some overflow issues, and can send bad frames */
			continue;
		}

		mcu = (Mcu*)seg.data;
		mcu_nr = mcu_seq(mcu);
		seq_delta = (seg.seq - last_seq + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;

		/* Compensate for lost MCUs */
		if (last_seq >= 0 && seq_delta > 1) {
			align_okay = 0;
			skip_zero = 1;

			while (!align_okay) {
				/* Realign all channels, filling with black MCUs until one of
				 * them is aligned to the current sequence number */
				for (ch=0; ch<3; ch++) {
					cur_gen = pxgen[ch];

					/* If the channel is new, don't try to align it */
					if (cur_gen->pkt_end < 0) {
						continue;
					}

					/* If the current channel is already aligned, skip it */
					if (skip_zero && cur_gen->mcu_nr == 0) {
						continue;
					}

					/* If we lost the current strip's end, pad with black */
					if (cur_gen->pkt_end < seg.seq && cur_gen->pkt_end - seg.seq < MPDU_MAX_SEQ/2) {
						printf("\nAligning %d %d", seg.seq, cur_gen->pkt_end);

						for (i=cur_gen->mcu_nr; i<MCU_PER_PP; i += MCU_PER_MPDU) {
							pixelgen_append(cur_gen, NULL);
						}

						cur_gen->mcu_nr = 0;
						cur_gen->pkt_end = (cur_gen->pkt_end + 3*MPDU_PER_LINE+1) % MPDU_MAX_SEQ;
					} else {
						/* Current channel is aligned: exit the while loop */
						align_okay = 1;
					}
				}

				skip_zero = 0;
				if (!align_okay) {
					bmp_newstrip(bmp);
				}
			}
		}

		/* Go to the next strip if the timestamp has changed */
		if (seg.timestamp > last_tstamp)  {
			bmp_newstrip(bmp);
		}

		/* Append the received MCU */
		for (ch=0; ch<3; ch++) {
			if (seg.apid == apid_list[ch]) {
				cur_gen = pxgen[ch];
				for (i = cur_gen->mcu_nr; i < mcu_nr; i += MCU_PER_MPDU) {
					pixelgen_append(cur_gen, NULL);
				}
				pixelgen_append(cur_gen, &seg);
			}
		}

		last_tstamp = seg.timestamp;
		last_seq = seg.seq;
	}

	printf("\nPacket count: %d/%d\n", valid_count, total_count);

	bmp_close(bmp);
	pkt_deinit(pp);
	for (ch=0; ch<3; ch++) {
		pixelgen_deinit(pxgen[ch]);
	}
	viterbi->close(viterbi);
	correlator->close(correlator);
	src->close(src);

	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

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
#include "reedsolomon.h"
#include "strip.h"
#include "utils.h"
#include "viterbi.h"

int
main(int argc, char *argv[])
{
	int i, j, line, c, chnum;
	unsigned int last_tstamp;
	int total_count, valid_count, mcu_nr, lines_delta, seq_delta, last_seq;
	int helper_seq;
	SoftSource *src, *correlator;
	HardSource *viterbi;
	Packetizer *pp;
	Channel *ch[3], *cur_ch;
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

	ch[0] = channel_init(apid_list[0]);
	ch[1] = channel_init(apid_list[1]);
	ch[2] = channel_init(apid_list[2]);

	last_seq = -1;
	last_tstamp = -1;
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

		/* Meteor-M2 has some overflow issues, and can send bad frames, which
		 * screw with the sequence numbering (they all have seq=0).Skip them. */
		if (seg.apid == 0) {
			continue;
		}

		mcu = (Mcu*)seg.data;
		mcu_nr = mcu_seq(mcu);
		seq_delta = (seg.seq - last_seq + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;

		/* Compensate for lost MCUs */
		if (last_seq >= 0 && seq_delta > 1) {
			printf("\nSeq delta: %d", seq_delta);

			for (chnum=0; chnum<3; chnum++) {
				cur_ch = ch[chnum];

				if (cur_ch->last_seq < 0) {
					continue;
				}

				helper_seq = (cur_ch->last_seq > seg.seq) ? seg.seq + MPDU_MAX_SEQ : seg.seq;
				printf("\nAligning %d -> %d (ch %d)", cur_ch->last_seq, helper_seq, chnum);

				/* Fill the end of the last line */
				while (cur_ch->mcu_offset > 0) {
					if (cur_ch->last_seq == helper_seq - 1) {
						break;
					}
					channel_decode(cur_ch, NULL);
					cur_ch->last_seq++;
				}

				/* Fill the middle rows if necessary */
				while (cur_ch->last_seq + MPDU_PER_PERIOD < helper_seq - 1) {
					channel_newline(cur_ch);
					cur_ch->last_seq += MPDU_PER_PERIOD;
				}
				cur_ch->last_seq %= MPDU_MAX_SEQ;
			}
		}

		/* Keep the uninitialized channels aligned */
		if (last_tstamp < seg.timestamp) {
			for (i=0; i<3; i++) {
				cur_ch = ch[i];

				/* Channel is uninitialized: check whether there is another
				 * channel that is initialized */
				if (cur_ch->last_seq < 0) {
					for (j=0; j<3; j++) {
						if (ch[j]->last_seq > 0) {
							/* Compute the number of lines to fill */
							lines_delta = (seg.seq - ch[j]->last_seq + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;
							lines_delta /= MPDU_PER_PERIOD;

							for (; lines_delta >= 0; lines_delta--) {
								channel_newline(cur_ch);
							}

							break;
						}
					}
				}
			}
		}

		/* Append the received MCU */
		for (i=0; i<3; i++) {
			if (seg.apid == apid_list[i]) {
				cur_ch = ch[i];

				/* Align the beginning of the row */
				while (cur_ch->mcu_offset < mcu_nr) {
					printf("\nMCU delta: %d -> %d", cur_ch->mcu_offset, mcu_nr);
					channel_decode(cur_ch, NULL);
				}
				channel_decode(cur_ch, &seg);
			}
		}

		last_tstamp = seg.timestamp;
		last_seq = seg.seq;
	}

	BmpSink *red = bmp_open("/tmp/red.bmp", 1568);
	BmpSink *green = bmp_open("/tmp/green.bmp", 1568);
	BmpSink *blue = bmp_open("/tmp/blue.bmp", 1568);

	for (i=0; i<ch[0]->len; i += 64) {
		if (bmp_append(red, ch[0]->data + i, ALL) >= 1568)
			bmp_newstrip(red);
	}
	for (i=0; i<ch[1]->len; i += 64) {
		if (bmp_append(green, ch[1]->data + i, ALL) >= 1568)
			bmp_newstrip(green);
	}
	for (i=0; i<ch[2]->len; i += 64) {
		if (bmp_append(blue, ch[2]->data + i, ALL) >= 1568)
			bmp_newstrip(blue);
	}

	bmp_close(red);
	bmp_close(green);
	bmp_close(blue);

	printf("\nPacket count: %d/%d\n", valid_count, total_count);

	pkt_deinit(pp);
	for (i=0; i<3; i++) {
		channel_deinit(ch[i]);
	}
	viterbi->close(viterbi);
	correlator->close(correlator);
	src->close(src);

	if (free_fname_on_exit) {
		free(out_fname);
	}
	return 0;
}

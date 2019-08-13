#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "channel.h"
#include "correlator.h"
#include "diff.h"
#include "file.h"
#include "options.h"
#include "packetizer.h"
#include "png_out.h"
#include "reedsolomon.h"
#include "utils.h"
#include "viterbi.h"

static void align_channels(Channel *ch[3], int seq);
static void align_uninit(Channel *ch[3], int seq);

int
main(int argc, char *argv[])
{
	int i, c;
	int write_statfile;
	unsigned int first_tstamp, last_tstamp;
	int total_count, valid_count;
	int mcu_nr, seq_delta, last_seq;
	uint8_t encoded_syncword[2*sizeof(SYNCWORD)];
	char *stat_fname;
	FILE *out_fd = NULL, *stat_fd = NULL;

	SoftSource *src, *diff, *correlator;
	HardSource *viterbi;
	Packetizer *pp;
	Channel *ch[3], *cur_ch;
	Segment seg;
	Mcu *mcu;


	/* Command-line changeable variables {{{*/
	int apid_list[3];
	int diffcoding;
	char *out_fname, *in_fname;
	int free_fname_on_exit;
	int quiet;
	/*}}}*/
	/* Initialize command-line overridable parameters {{{*/
	quiet = 0;
	out_fname = NULL;
	free_fname_on_exit = 0;
	write_statfile = 0;
	diffcoding = 0;
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
		case 'd':
			diffcoding = 1;
			break;
		case 'h':
			usage(argv[0]);
			break;
		case 'o':
			out_fname = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		case 's':
			write_statfile = 1;
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

	if (diffcoding) {
		diff = diff_src(src);
		correlator = correlator_init_soft(diff, encoded_syncword);
	} else {
		correlator = correlator_init_soft(src, encoded_syncword);
	}

	viterbi = viterbi_init(correlator);
	pp = pkt_init(viterbi);

	ch[0] = channel_init(apid_list[0]);
	ch[1] = channel_init(apid_list[1]);
	ch[2] = channel_init(apid_list[2]);

	/* Open/create the output PNG */
	if (!(out_fd = fopen(out_fname, "w"))) {
		fatal("Could not create/open output file");
	}

	if (write_statfile) {
		stat_fname = safealloc(strlen(out_fname) + 5 + 1);
		sprintf(stat_fname, "%s.stat", out_fname);
		if (!(stat_fd = fopen(stat_fname, "w"))) {
			fatal("Could not create/open stat file");
		}
	}

	last_seq = -1;
	first_tstamp = -1;
	last_tstamp = -1;
	total_count = 0;
	valid_count = 0;

	/* Read all the packets */
	printf("Decoding started\n");
	while (pkt_read(pp, &seg)) {
		total_count++;

		if (!quiet) {
			printf("\033[2K\r");
			printf("0x%08X rs=%2d ", pkt_get_marker(pp), pkt_get_rs(pp));
		}

		/* Skip invalid packets */
		if (seg.len <= 0) {
			fflush(stdout);
			continue;
		}

		valid_count++;

		if (!quiet) {
			printf("seq=%5d, APID %d  %s\r", seg.seq, seg.apid, timeofday(seg.timestamp));
			fflush(stdout);
		}

		/* Meteor-M2 has some overflow issues, and can send bad frames, which
		 * screw with the sequence numbering (they all have seq=0).Skip them. */
		if (seg.apid == 0) {
			continue;
		}

		if (last_seq < 0) {
			first_tstamp = seg.timestamp;
		}

		mcu = (Mcu*)seg.data;
		mcu_nr = mcu_seq(mcu);
		seq_delta = (seg.seq - last_seq + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;

		/* Compensate for lost MCUs */
		if (last_seq >= 0 && seq_delta > 1) {
			align_channels(ch, seg.seq);
		}

		/* Keep the uninitialized channels aligned */
		if (last_tstamp < seg.timestamp) {
			align_uninit(ch, seg.seq);
		}

		/* Append the received MCU */
		for (i=0; i<3; i++) {
			if (seg.apid == apid_list[i]) {
				cur_ch = ch[i];

				/* Align the beginning of the row */
				while (cur_ch->mcu_offset < mcu_nr) {
					channel_decode(cur_ch, NULL);
				}
				channel_decode(cur_ch, &seg);
			}
		}

		last_tstamp = seg.timestamp;
		last_seq = seg.seq;
	}
	if (!quiet) {
		printf("\n");
	}
	printf("Decoding complete\n");
	printf("Successfully decoded packets: %d/%d (%4.1f%%)\n", valid_count, total_count,
			100.0 * valid_count/total_count);

	pkt_deinit(pp);
	viterbi->close(viterbi);
	correlator->close(correlator);
	src->close(src);

	/* Write the 3 decoded channels to a PNG if there's any data in them */
	if (valid_count > 0) {
		png_compose(out_fd, ch[0], ch[1], ch[2]);

		/* Write the .stat file used by software like MeteorGIS */
		if (stat_fd) {
			fprintf(stat_fd, "%s\r\n", timeofday(first_tstamp));
			fprintf(stat_fd, "%s\r\n", timeofday(last_tstamp - first_tstamp));
			fprintf(stat_fd, "0,1538925\r\n");
		}
	}


	/* Deinitialize/free all allocated resources */
	for (i=0; i<3; i++) {
		channel_deinit(ch[i]);
	}
	if (out_fd) {
		fclose(out_fd);
	}

	if (stat_fd) {
		fclose(stat_fd);
	}
	if (free_fname_on_exit) {
		free(out_fname);
	}

	return 0;
}

/* Static functions {{{ */
/* Align channels to a specific segment number */
void
align_channels(Channel *ch[3], int seq)
{
	Channel *cur_ch;
	int chnum;
	int helper_seq;

	for (chnum=0; chnum<3; chnum++) {
		cur_ch = ch[chnum];

		if (cur_ch->last_seq < 0) {
			continue;
		}

		/* Compute the target sequence number to reach */
		helper_seq = (cur_ch->last_seq > seq) ? seq + MPDU_MAX_SEQ : seq;

		/* Fill the end of the last line */
		while (cur_ch->mcu_offset > 0) {
			if (cur_ch->last_seq == helper_seq - 1) {
				break;
			}
			channel_decode(cur_ch, NULL);
			cur_ch->last_seq++;
		}

		/* Fill the middle rows if necessary */
		while (cur_ch->last_seq + MPDU_PER_PERIOD < helper_seq) {
			channel_newline(cur_ch);
			cur_ch->last_seq += MPDU_PER_PERIOD;
		}
		cur_ch->last_seq %= MPDU_MAX_SEQ;
	}
}

/* Maintain the beginning of each channel aligned */
void
align_uninit(Channel *ch[3], int seq)
{
	int i, j, lines_delta;
	Channel *cur_ch;

	for (i=0; i<3; i++) {
		cur_ch = ch[i];

		/* Channel is uninitialized: check whether there is another
		 * channel that is initialized */
		if (cur_ch->last_seq < 0) {
			for (j=0; j<3; j++) {
				if (ch[j]->last_seq > 0) {
					/* Compute the number of lines to fill */
					lines_delta = (seq - ch[j]->last_seq + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;
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
/*}}}*/

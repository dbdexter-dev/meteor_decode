#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include "channel.h"
#include "decode.h"
#include "output/bmp_out.h"
#include "parser/mcu_parser.h"
#include "raw_channel.h"
#include "utils.h"

#ifdef USE_PNG
#include "output/png_out.h"
#endif

#define CLR "\033[1K\r"

#define NUM_CHANNELS 3
#define MAX_FNAME_LEN 256
#define SHORTOPTS "7a:bdhio:qstv"

static int read_wrapper(int8_t *src, size_t len);
static int preferred_channel(int apid);
static int parse_apids(int *apids, char *optarg);
static void process_mpdu(Mpdu *mpdu, Channel *ch[NUM_CHANNELS], RawChannel *apid_70);
static void write_stat_and_close(FILE *fd);

static FILE *_soft_file;
static int _quiet;
static uint64_t _first_time, _last_time;

static struct option longopts[] = {
	{ "70",      0, NULL, '7' },
	{ "apid",    1, NULL, 'a' },
	{ "batch",   0, NULL, 'b' },
	{ "diff",    0, NULL, 'd' },
	{ "help",    0, NULL, 'h' },
	{ "int",     0, NULL, 'i' },
	{ "output",  1, NULL, 'o' },
	{ "quiet",   0, NULL, 'q' },
	{ "split",   0, NULL, 's' },
	{ "statfile",0, NULL, 't' },
	{ "version", 0, NULL, 'v' },
};

int
main(int argc, char *argv[])
{
	char *input_fname, *output_fname=NULL, *extension;
	char split_fname[MAX_FNAME_LEN], stat_fname[MAX_FNAME_LEN], apid_70_fname[MAX_FNAME_LEN];
	char auto_out_fname[MAX_FNAME_LEN];
	size_t file_len;
	int mpdu_count=0, height;
	float percent;
	int diffcoded, interleaved, split_output, write_stat, write_apid_70, fancy_output;
	int i, j, c, retval;
	int duplicate;
	Mpdu mpdu;
	Channel ch_instance[NUM_CHANNELS], *ch[NUM_CHANNELS];
	RawChannel ch_apid_70;
	DecoderState status;
	void *img_out;
	int apids[NUM_CHANNELS];

	/* Pointers to functions to initialize, write and close images (will point
	 * to different functions based on the output format) */
	int (*img_init)(void **img, const char *fname, int width, int height, int mono);
	int (*img_write_rgb)(void *img, Channel *red, Channel *green, Channel *blue);
	int (*img_write_mono)(void *img, Channel *ch);
	int (*img_finalize)(void *img);

	/* Default parameters {{{ */
	apids[0] = apids[1] = apids[2] = -1;

	diffcoded = 0;
	interleaved = 0;
	fancy_output = 1;
	split_output = 0;
	write_stat = 0;
	write_apid_70 = 0;
	_quiet = 0;
	/* }}} */
	/* Parse command-line options {{{ */
	optind = 0;
	while ((c = getopt_long(argc, argv, SHORTOPTS, longopts, NULL)) != -1) {
		switch (c) {
			case '7':
				write_apid_70 = 1;
				break;
			case 'a':
				if (parse_apids(apids, optarg)) {
					fprintf(stderr, "Invalid APIDs specified\n");
					usage(argv[0]);
					exit(1);
				}
				break;
			case 'b':
				fancy_output = 0;
				break;
			case 'o':
				output_fname = optarg;
				break;
			case 'd':
				diffcoded = 1;
				break;
			case 'i':
				interleaved = 1;
				break;
			case 's':
				split_output = 1;
				break;
			case 'q':
				_quiet = 1;
				break;
			case 'h':
				usage(argv[0]);
				return 0;
			case 'v':
				version();
				return 0;
			case 't':
				write_stat = 1;
				break;
			default:
				usage(argv[0]);
				return 1;
		}
	}

	if (argc - optind < 1) {
		usage(argv[0]);
		return 1;
	}

	input_fname = argv[optind];

	/* If no output filename is specified, make one up based on the input file */
	if (!output_fname) {
		/* If the input is stdin, use a generic output filename */
		if (!strcmp(input_fname, "-")) {
			sprintf(auto_out_fname, "lrpt_out.bmp");
		} else {

			/* Find where extension starts */
			for (i = strlen(input_fname); i>=0 && input_fname[i] != '.'; i--);
			if (i < 0) i = strlen(input_fname);

			if (i > MAX_FNAME_LEN - 4) {
				fprintf(stderr, "Automatic filename too long, please specify a different filename\n");
				usage(argv[0]);
				return 1;
			}

			/* Copy input file name without extension, add ".bmp" extension to it */
			memcpy(auto_out_fname, input_fname, i);
			sprintf(auto_out_fname + i, ".bmp");
		}

		output_fname = auto_out_fname;
	}

	img_init = bmp_init;
	img_write_rgb = bmp_write_rgb;
	img_write_mono = bmp_write_mono;
	img_finalize = bmp_finalize;
#ifdef USE_PNG
	/* If the image extension is .png, change image write pointers */
	if (!strncmp(output_fname+strlen(output_fname)-4, ".png", 4)) {
		img_init = png_init;
		img_write_rgb = png_write_rgb;
		img_write_mono = png_write_mono;
		img_finalize = png_finalize;
	}
#endif
	/* }}} */

	/* Open input file */
	if (!strcmp(input_fname, "-")) {
		_soft_file = stdin;
	} else if (!(_soft_file = fopen(input_fname, "rb"))) {
		fprintf(stderr, "Could not open input file\n");
		return 1;
	}

	/* Get file length */
	fseek(_soft_file, 0, SEEK_END);
	file_len = ftell(_soft_file);
	fseek(_soft_file, 0, SEEK_SET);

	/* Initialize channels, duping pointers when two APIDs are the same */
	for (i=0; i<NUM_CHANNELS; i++) {
		ch[i] = NULL;
		for (j=0; j<i; j++) {
			if (apids[i] == apids[j]) {
				ch[i] = ch[j];
				break;
			}
		}

		if (!ch[i] || apids[i] == -1) {
			channel_init(&ch_instance[i], apids[i]);
			ch[i] = &ch_instance[i];
		}
	}

	/* Open APID 70 output file if necessary */
	if (write_apid_70) {
		sprintf(apid_70_fname, "%s.70", output_fname);
		raw_channel_init(&ch_apid_70, apid_70_fname);
	}

	/* Initialize decoder */
	decode_init(diffcoded, interleaved);

	/* Main processing loop {{{ */
	while ((status = decode_soft_cadu(&mpdu, &read_wrapper)) != EOF_REACHED) {
		/* If the MPDU was parsed, or if the MPDU cannot be parsed (due to too
		 * many errors, invalid fields etc.), print a new status line */
		if (!_quiet && (status == MPDU_READY || status == STATS_ONLY)) {
			printf(fancy_output ? CLR : "\n");
			percent = 100.0*(float)ftell(_soft_file)/file_len;
			printf("%6.2f%% vit(avg): %4d rs(sum): %2d",
					percent,
					decode_get_vit(), decode_get_rs());
		}

		if (status == MPDU_READY) {
			/* Process decoded MPDUs */
			process_mpdu(&mpdu, ch, write_apid_70 ? &ch_apid_70 : NULL);
			mpdu_count++;
		}

		fflush(stdout);
	}
	/* }}} */

	height = MAX(ch[0]->offset, MAX(ch[1]->offset, ch[2]->offset))
	       / (MCU_PER_LINE*8);

	if (!_quiet) printf(fancy_output ? CLR : "\n\n");
	printf("MPDUs received: %d (%d lines)\n", mpdu_count, height);
	printf("Onboard time elapsed: %s\n", mpdu_time(_last_time - _first_time));

	/* If at least one line was received, write output image(s) {{{ */
	if (height) {
		if (split_output) {
			/* Separate file extension from the rest of the file name */
			for (extension=output_fname + strlen(output_fname); *extension != '.' && extension > output_fname; extension--);
			if (extension == output_fname) {
				extension = output_fname + strlen(output_fname);
			} else {
				*extension++ = '\0';
			}

			/* Write each channel separately */
			for (i=0; i<NUM_CHANNELS; i++) {
				/* If we've already written this channel, don't do it again */
				duplicate = 0;
				for (j=0; j<i; j++) {
					if (ch[i]->apid == ch[j]->apid) {
						duplicate = 1;
						break;
					}
				}
				if (duplicate) continue;

				/* Generate a filename for the current channel */
				sprintf(split_fname, "%s_%02d%s%s", output_fname, ch[i]->apid, *extension ? "." : "", extension);
				printf("Saving channel to %s... ", split_fname);
				fflush(stdout);

				/* Dump channel to monochrome image */
				retval  = img_init(&img_out, split_fname, MCU_PER_LINE*8, height, 1);
				retval |= img_write_mono(img_out, ch[i]);
				retval |= img_finalize(img_out);

				if (retval) {
					printf("Failed!\n");
					return 1;
				}

				/* Write .stat file if necessary */
				if (write_stat) {
					sprintf(stat_fname, "%s_%02d.stat", output_fname, ch[i]->apid);
					write_stat_and_close(fopen(stat_fname, "wb"));
				}
				printf("Done.\n");
			}

		} else {
			printf("Saving composite to %s... ", output_fname);
			fflush(stdout);

			/* Write composite RGB */
			retval  = img_init(&img_out, output_fname, MCU_PER_LINE*8, height, 0);
			retval |= img_write_rgb(img_out, ch[0], ch[1], ch[2]);
			retval |= img_finalize(img_out);

			if (retval) {
				printf("Failed!\n");
				return 1;
			}
			printf("Done.\n");

			if (write_stat) {
				sprintf(stat_fname, "%s.stat", output_fname);
				write_stat_and_close(fopen(stat_fname, "wb"));
			}
		}
	}
	/*}}}*/

	/* Cleanup */
	for (i=0; i<NUM_CHANNELS; i++) {
		channel_close(ch[i]);
	}
	if (_soft_file != stdin) fclose(_soft_file);
	if (write_apid_70) raw_channel_close(&ch_apid_70);

	return 0;
}

static int
read_wrapper(int8_t *dst, size_t len)
{
	return fread(dst, len, 1, _soft_file);
}

static void
process_mpdu(Mpdu *mpdu, Channel *ch[NUM_CHANNELS], RawChannel *apid_70)
{
	static int first = 1;
	unsigned int seq, apid, lines_lost;
	uint8_t strip[MCU_PER_MPDU][8][8];
	uint64_t time;
	int i;

	seq = mpdu_seq(mpdu);
	apid = mpdu_apid(mpdu);
	time = mpdu_raw_time(mpdu);

	/* Print status line */
	if (!_quiet) {
		printf("\tAPID %2d  seq: %d  %s",
				apid, decode_get_vcdu_seq(), mpdu_time(time));
	}

	if (first) _first_time = time;

	/* When sat reboots, its time is highly unreliable, and can jump backwards
	 * which makes things... weird. Handle that case by discarding the packets */
	if (time < _first_time && _first_time - time < US_PER_DAY/2) {
		printf(" Invalid timestamp");
		return;
	}

	_last_time = time;

	/* Parse packet based on the APID */
	switch (apid) {
		case 64:
		case 65:
		case 66:
		case 67:
		case 68:
		case 69:
			/* Map APID to channel. In order:
			 * - Use the channel with the APID of this packet
			 * - If no channel has the current APID, see if the one preferred by
			 *   this APID is free and use it
			 * - If the preferred channel isn't free, pick the first one that is
			 *   free
			 * - If all else fails, discard this packet
			 */
			for (i=0; i<NUM_CHANNELS && ch[i]->apid != (int)apid; i++);
			if (i == NUM_CHANNELS) {
				i = preferred_channel(apid);
				if (ch[i]->apid < 0) {
					ch[i]->apid = apid;
				} else {
					for (i=0; i<NUM_CHANNELS && ch[i]->apid >= 0; i++);
					if (i == NUM_CHANNELS) break;
					ch[i]->apid = apid;
				}
			}
			if (i == NUM_CHANNELS || ch[i]->apid != (int)apid) break;


			/* AVHRR image data: decode JPEG into raw pixel data */
			avhrr_decode(strip, &mpdu->data.mcu.avhrr, mpdu_len(mpdu));

			/* Estimate number of lines lost compared to other channels based on
			 * timestamps, and compensate for those */
			if (ch[i]->mpdu_seq < 0) {
				lines_lost = (_last_time - _first_time) / MPDU_US_PER_LINE;
				ch[i]->mpdu_seq = (seq - MPDU_PER_PERIOD*lines_lost - 1 + MPDU_MAX_SEQ) % MPDU_MAX_SEQ;
			}

			/* Append decoded strip */
			channel_append_strip(ch[i], strip, mpdu->data.mcu.avhrr.seq, seq);
			break;

		case 70:
			/* AVHRR calibration data: directly write to file */
			raw_channel_write(apid_70, (uint8_t*)mpdu,
					 sizeof(mpdu->id) + sizeof(mpdu->seq) + sizeof(mpdu->len)
				   + sizeof(mpdu->data.time) + sizeof(mpdu->data.mcu.calib.data));
			break;

		default:
			break;
	}

	if (first) first = 0;
}

static int
parse_apids(int *apids, char *optarg)
{
	int i, red, green, blue;

	if (sscanf(optarg, "%d,%d,%d", &red, &green, &blue) != 3) return 1;

	apids[0] = red;
	apids[1] = green;
	apids[2] = blue;

	for (i=0; i<3; i++) {
		if (apids[i] < 64 || apids[i] > 69) return 1;
	}

	return 0;
}

static int
preferred_channel(int apid)
{
	switch (apid) {
		case 64: return 2;
		case 65: return 1;
		case 66: return 0;
		case 67: return 1;
		case 68: return 0;
		case 69: return 2;
		default: return 0;
	}
}

static void
write_stat_and_close(FILE *fd)
{
	if (!fd) return;

	fprintf(fd, "%s\r\n", mpdu_time(_first_time));
	fprintf(fd, "%s\r\n", mpdu_time(_last_time - _first_time));
	fprintf(fd, "0\r\n");  /* Not sure what this is? */
	fclose(fd);
}

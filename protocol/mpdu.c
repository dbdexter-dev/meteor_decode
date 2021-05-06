#include <stdio.h>
#include <stdint.h>
#include "mpdu.h"

static char _mpdu_time[sizeof("HH:MM:SS.mmm  ")];

extern inline uint8_t  mpdu_version(Mpdu *m);
extern inline uint8_t  mpdu_type(Mpdu *m);
extern inline uint8_t  mpdu_has_secondary_hdr(Mpdu *m);
extern inline uint16_t mpdu_apid(Mpdu *m);
extern inline uint8_t  mpdu_seq_flag(Mpdu *m);
extern inline uint16_t mpdu_seq(Mpdu *m);
extern inline uint16_t mpdu_len(Mpdu *m);
extern inline uint16_t mpdu_day(Mpdu *m);
extern inline uint32_t mpdu_ms(Mpdu *m);
extern inline uint16_t mpdu_us(Mpdu *m);
extern inline uint64_t mpdu_raw_time(Mpdu *m);

char*
mpdu_time(uint64_t us)
{
	unsigned hr, min, sec, ms;

	ms = us / 1000;
	hr = ms / 1000 / 60 / 60 % 24;
	min = ms / 1000 / 60 % 60;
	sec = ms / 1000 % 60;
	ms %= 1000;

	sprintf(_mpdu_time, "%02d:%02d:%02d.%03d", hr, min, sec, ms);
	return _mpdu_time;
}

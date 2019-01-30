#include <stdio.h>
#include <stdint.h>
#include "packet.h"

static int vcdu_header_present(const Cvcdu *p);

/* From packet.h */
const uint8_t SYNCWORD[] = {0x1a, 0xcf, 0xfc, 0x1d};


/* VCDU-oriented functions */
void*
vcdu_header_ptr(Cvcdu *p)
{
	if(vcdu_header_present(p)) {
		return (uint8_t*)&p->mpdu_data + vcdu_header_offset(p);
	}
	return NULL;
}

uint32_t
vcdu_counter(const Cvcdu *p)
{
	return (p->counter[0] << 16) | (p->counter[1] << 8) | (p->counter[2]);
}

int
vcdu_id(const Cvcdu *p)
{
	return p->info[1] & 0x3F;
}

int 
vcdu_spacecraft(const Cvcdu *p)
{
	return (p->info[0] & 0x3F) << 2 | (p->info[1] >> 5);
}

int
vcdu_vcid(const Cvcdu *p)
{
	return p->info[1] & 0x3F;
}



/* MPDU-oriented functions */
int
mpdu_apid(const Mpdu *p)
{
	return (p->id[0] & 0x07) << 8 | p->id[1];
}

int
mpdu_grouping(const Mpdu *p)
{
	return p->seq_ctrl[0] >> 6;
}

uint32_t
mpdu_msec(const Mpdu *p)
{
	return p->time.msec[0] << 24 | p->time.msec[1] << 16 | 
	       p->time.msec[2] << 8 | p->time.msec[3];
}

uint16_t
mpdu_len(const Mpdu *p)
{
	return p->len[0] << 8 | p->len[1];
}

int
vcdu_header_offset(const Cvcdu *p)
{
	return (p->mpdu_header[0] & 0x07) << 8 | p->mpdu_header[1];
}

/* Static functions {{{*/
static int
vcdu_header_present(const Cvcdu *p)
{
	return !(p->mpdu_header[0] >> 3);
}

/*}}}*/

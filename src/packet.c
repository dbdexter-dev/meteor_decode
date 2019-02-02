#include <stdio.h>
#include <stdint.h>
#include "packet.h"

static int vcdu_header_present(const Cvcdu *p);

/* From packet.h */
const uint8_t SYNCWORD[] = {0x1a, 0xcf, 0xfc, 0x1d};


/* VCDU-oriented functions */
int
vcdu_header_offset(const Cvcdu *p)
{
	return (p->mpdu_header[0] & 0x07) << 8 | p->mpdu_header[1];
}

void*
vcdu_mpdu_header_ptr(Cvcdu *p)
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
mpdu_data_len(const Mpdu *p)
{
	uint16_t len;

	len = mpdu_raw_len(p) + 1;
	return len;
}

uint8_t*
mpdu_data_ptr(const Mpdu *p)
{
	return (uint8_t*)p + MPDU_HDR_SIZE;
}

int
mpdu_grouping(const Mpdu *p)
{
	return p->seq_ctrl[0] >> 6;
}

int
mpdu_has_sec_hdr(const Mpdu *p)
{
	return p->id[0] & 0x08;
}

uint32_t
mpdu_msec(const Timestamp *t)
{
	return t->msec[0] << 24 | t->msec[1] << 16 | t->msec[2] << 8 | t->msec[3];
}

int
mpdu_raw_len(const Mpdu *p)
{
	return (p->len[0] << 8 | p->len[1]);
}

int
mpdu_seq(const Mpdu *p)
{
	return (p->seq_ctrl[0] & 0x3F) << 8 | p->seq_ctrl[1];
}

/* MCU-oriented functions */
void*
mcu_data_ptr(Mcu *p)
{
	return &p->data;
}

void*
mcu_hk_data_ptr(const McuHK *p)
{
	return (void*)&p->data;
}

int
mcu_quant_table(const Mcu *p)
{
	return p->scan_hdr[0];
}

int
mcu_seq(const Mcu *p)
{
	return p->seq;
}

int
mcu_huffman_ac_idx(const Mcu *p)
{
	return p->scan_hdr[1] & 0x0F;
}

int
mcu_huffman_dc_idx(const Mcu *p)
{
	return p->scan_hdr[1] >> 4;
}

int
mcu_quality_factor(const Mcu *p)
{
	return p->segment_hdr[2];
}



/* Static functions {{{*/
static int
vcdu_header_present(const Cvcdu *p)
{
	return !(p->mpdu_header[0] >> 3);
}

/*}}}*/

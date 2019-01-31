/** 
 * LRPT packet structure information, plus some auxiliary functions to make
 * accessing weird bit offsets simpler
 */

#ifndef LRPTDEC_PACKET_H
#define LRPTDEC_PACKET_H

#include <stdint.h>

#define FRAME_SIZE (sizeof(Cadu))
#define FRAME_BITS (FRAME_SIZE << 3)
#define SOFT_FRAME_SIZE (FRAME_BITS * 2)
#define INTERLEAVING 4

#define VCDU_RS_SIZE 128

#define MPDU_SEC_HDR_SIZE 8
#define MPDU_HDR_SIZE 6
#define MPDU_DATA_SIZE 882

#define MCU_HDR_SIZE 6

/* Minimum code unit structure */
typedef struct {
	struct {
		uint8_t day[2];
		uint8_t msec[4];
		uint8_t usec[2];
	} timestamp;

	uint8_t seq;
	uint8_t scan_hdr[2];
	uint8_t segment_hdr[3];

	uint8_t data;
} Mcu;

typedef struct {
	struct {
		uint8_t day[2];
		uint8_t msec[4];
		uint8_t usec[2];
	} timestamp;

	uint8_t data;
} McuHK;

/* Generic reconstructed MPDU, NOT 882 bytes long */
typedef struct {
	uint8_t id[2];
	uint8_t seq_ctrl[2];
	uint8_t len[2];

	uint8_t data;
} Mpdu;

/* Coded Virtual Channel Data Unit */
typedef struct {
	/* Primary header */
	uint8_t info[2]; 
	uint8_t counter[3];
	uint8_t signalling;
	uint8_t coding_ctrl[2];

	/* Data unit zone */
	uint8_t mpdu_header[2];
	uint8_t mpdu_data[MPDU_DATA_SIZE];

	uint8_t reed_solomon[VCDU_RS_SIZE];
} Cvcdu;

/* Channel Access Data Unit */
typedef struct _cadu {
	uint32_t sync_marker;
	Cvcdu cvcdu;
} Cadu;

/* Defined in packet.c */
const uint8_t SYNCWORD[4];

uint32_t vcdu_counter(const Cvcdu *p);
int      vcdu_header_offset(const Cvcdu *p);
int      vcdu_id(const Cvcdu *p);
void*    vcdu_mpdu_header_ptr(Cvcdu *p);
int      vcdu_spacecraft(const Cvcdu *p);
int      vcdu_vcid(const Cvcdu *p);

int      mpdu_apid(const Mpdu *p);
int      mpdu_data_len(const Mpdu *p);
uint8_t* mpdu_data_ptr(const Mpdu *p);
int      mpdu_grouping(const Mpdu *p);
int      mpdu_has_sec_hdr(const Mpdu *p);
int      mpdu_raw_len(const Mpdu *p);
int      mpdu_seq(const Mpdu *p);

void*    mcu_data_ptr(const Mcu *p);
int      mcu_huffman_ac_idx(const Mcu *p);
int      mcu_huffman_dc_idx(const Mcu *p);
int      mcu_quality_factor(const Mcu *p);
int      mcu_quant_table(const Mcu *p);
void*    mcu_hk_data_ptr(const McuHK *p);


#endif


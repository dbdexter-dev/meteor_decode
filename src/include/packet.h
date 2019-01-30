/** 
 * LRPT packet structure information, plus some auxiliary functions to make
 * accessing weird bit offsets simpler
 */

#ifndef LRPTDEC_PACKET_H
#define LRPTDEC_PACKET_H

#include <stdint.h>

#define FRAME_SIZE (sizeof(struct _cadu))
#define FRAME_BITS (FRAME_SIZE << 3)
#define SOFT_FRAME_SIZE (FRAME_BITS * 2)
#define INTERLEAVING 4
#define MCU_SIZE 64

typedef struct {
	uint8_t day[2];
	uint8_t msec[4];
	uint8_t usec[2];
} Timestamp;

/* Generic reconstructed MPDU, NOT 882 bytes long */
typedef struct {
	uint8_t id[2];
	uint8_t seq_ctrl[2];
	uint8_t len[2];

	Timestamp time;

	uint8_t data[6];
	uint8_t data_two;
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
	uint8_t mpdu_data[882];

	uint8_t reed_solomon[128];
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
void*    vcdu_header_ptr(Cvcdu *p);
int      vcdu_id(const Cvcdu *p);
int      vcdu_spacecraft(const Cvcdu *p);
int      vcdu_vcid(const Cvcdu *p);

int      mpdu_apid(const Mpdu *p);
int      mpdu_grouping(const Mpdu *p);
uint32_t mpdu_msec(const Mpdu *p);
uint16_t mpdu_len(const Mpdu *p);

#endif


#ifndef vcdu_h
#define vcdu_h

#include <stdint.h>

#define VCDU_DATA_LENGTH 882
#define VCDU_RS_LENGTH 128

/* Data link layer */
typedef struct {
	/* VCDU primary header */
	uint8_t primary_hdr[6];

	/* VCDU insert zone */
	uint8_t encryption;
	uint8_t encr_key_num;

	/* VCDU data unit zone */
	uint8_t mpdu_hdr[2];
	uint8_t mpdu_data[VCDU_DATA_LENGTH];

	/* RS check symbols over the entire VCDU contents */
	uint8_t checksum[VCDU_RS_LENGTH];
}__attribute__((packed)) Vcdu;

/* Data link layer accessors */
inline uint8_t vcdu_version(Vcdu *c) { return c->primary_hdr[0] >> 6; }
inline uint8_t vcdu_spacecraft_id(Vcdu *c) { return (c->primary_hdr[0] & 0x3F) << 2 | c->primary_hdr[1] >> 6;}
inline uint8_t vcdu_type(Vcdu *c) { return c->primary_hdr[1] & 0x3F; }
inline uint32_t vcdu_counter(Vcdu *c) { return c->primary_hdr[2] << 16 | c->primary_hdr[3] << 8 | c->primary_hdr[4]; }
inline uint8_t vcdu_replay(Vcdu *c) { return c->primary_hdr[5] >> 7; }
inline uint8_t vcdu_signalling_spare(Vcdu *c) { return c->primary_hdr[5] & 0x7F; }
inline uint8_t vcdu_encryption(Vcdu *c) { return c->encryption; }
inline uint8_t vcdu_encr_num(Vcdu *c) { return c->encr_key_num; }
inline uint8_t vcdu_mpdu_spare(Vcdu *c) { return c->mpdu_hdr[0] >> 3; }
inline uint16_t vcdu_header_ptr(Vcdu *c) { return (c->mpdu_hdr[0] & 0x7) << 8 | c->mpdu_hdr[1]; }
inline uint8_t vcdu_header_present(Vcdu *c) { return !vcdu_mpdu_spare(c) && vcdu_header_ptr(c) != 0x7FF; }
inline uint8_t* vcdu_data(Vcdu *c) { return c->mpdu_data; }
inline uint8_t* vcdu_checksum(Vcdu *c) { return c->checksum; }
#endif

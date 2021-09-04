#ifndef mcu_h
#define mcu_h

#include <stdint.h>

#define MCUSEG_MAX_DATA_LEN 2048
#define MCU_PER_MPDU 14

typedef struct {
	uint8_t seq;
	uint8_t scan_hdr[2];
	uint8_t segment_hdr[3];
	uint8_t data[MCUSEG_MAX_DATA_LEN-6];
}__attribute__((packed)) AVHRR;

typedef struct {
	uint8_t data[114];
	uint8_t pad[MCUSEG_MAX_DATA_LEN - 114];
}__attribute__((packed)) Calib;

typedef union {
	AVHRR avhrr;
	Calib calib;
}__attribute__((packed)) McuSegment;

inline uint8_t avhrr_seq(AVHRR *a) { return a->seq; }
inline uint8_t avhrr_quant_table(AVHRR *a) { return a->scan_hdr[0]; }
inline uint8_t avhrr_ac_idx(AVHRR *a) { return a->scan_hdr[1] & 0xF; }
inline uint8_t avhrr_dc_idx(AVHRR *a) { return a->scan_hdr[1] >> 4; }
inline uint8_t avhrr_q(AVHRR *a) { return a->segment_hdr[2]; }

#endif /* mcu_h */

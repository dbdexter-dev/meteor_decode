#ifndef mpdu_h
#define mpdu_h

#include <stdint.h>
#include "mcu.h"

#define MPDU_MAX_DATA_LEN sizeof(McuSegment)
#define MPDU_HDR_LEN (sizeof(Mpdu)-sizeof(McuSegment)-sizeof(Timestamp))
#define MPDU_MAX_SEQ 16384
#define MPDU_PER_LINE 14
#define MPDU_PER_PERIOD (3*MPDU_PER_LINE + 1)
#define MCU_PER_LINE (MCU_PER_MPDU * MPDU_PER_LINE)
#define MPDU_US_PER_LINE (1220*1000) /* Imprecise, lower bound only */
#define US_PER_DAY ((uint64_t)1000L * 1000L * 86400L)

typedef struct {
	uint8_t day[2];
	uint8_t ms[4];
	uint8_t us[2];
} __attribute__((packed)) Timestamp;

typedef struct {
	uint8_t id[2];
	uint8_t seq[2];
	uint8_t len[2];

	struct {
		Timestamp time;
		McuSegment mcu;
	}__attribute__((packed)) data;
} __attribute__((packed)) Mpdu;

inline uint8_t  mpdu_version(Mpdu *m) { return m->id[0] >> 5; }
inline uint8_t  mpdu_type(Mpdu *m) { return m->id[0] >> 4 & 0x1; }
inline uint8_t  mpdu_has_secondary_hdr(Mpdu *m) { return m->id[0] >> 3 & 0x1; }
inline uint16_t mpdu_apid(Mpdu *m) { return (m->id[0] & 0x7) << 8 | m->id[1]; }
inline uint8_t  mpdu_seq_flag(Mpdu *m) { return m->seq[0] >> 6; }
inline uint16_t mpdu_seq(Mpdu *m) { return (m->seq[0] & 0x3F) << 8 | m->seq[1]; }
inline uint16_t mpdu_len(Mpdu *m) { return (m->len[0] << 8 | m->len[1]) + 1; }
inline uint16_t mpdu_day(Mpdu *m) { return m->data.time.day[0] << 8 | m->data.time.day[1]; }
inline uint32_t mpdu_ms(Mpdu *m) { return (uint32_t)m->data.time.ms[0] << 24 | (uint32_t)m->data.time.ms[1] << 16 | (uint32_t)m->data.time.ms[2] << 8 | (uint32_t)m->data.time.ms[3]; }
inline uint16_t mpdu_us(Mpdu *m) { return m->data.time.us[0] << 8 | m->data.time.us[1]; }
inline uint64_t mpdu_raw_time(Mpdu *m) { return (uint64_t)mpdu_day(m)*86400LL*1000LL*1000LL + (uint64_t)mpdu_ms(m)*1000L + (uint64_t)mpdu_us(m); }

char *mpdu_time(uint64_t us);
#endif /* mpdu_h */

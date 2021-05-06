#include "vcdu.h"

extern inline uint8_t vcdu_version(Vcdu *c);
extern inline uint8_t vcdu_spacecraft_id(Vcdu *c);
extern inline uint8_t vcdu_type(Vcdu *c);
extern inline uint32_t vcdu_counter(Vcdu *c);
extern inline uint8_t vcdu_replay(Vcdu *c);
extern inline uint8_t vcdu_signalling_spare(Vcdu *c);
extern inline uint8_t vcdu_encryption(Vcdu *c);
extern inline uint8_t vcdu_encr_num(Vcdu *c);
extern inline uint8_t vcdu_mpdu_spare(Vcdu *c);
extern inline uint8_t vcdu_header_present(Vcdu *c);
extern inline uint16_t vcdu_header_ptr(Vcdu *c);
extern inline uint8_t* vcdu_data(Vcdu *c);
extern inline uint8_t* vcdu_checksum(Vcdu *c);

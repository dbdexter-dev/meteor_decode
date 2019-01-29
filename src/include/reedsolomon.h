/* See https://public.ccsds.org/Pubs/101x0b4s.pdf for more info on the
 * Reed-Solomon code parameters in use */

#ifndef LRPTDEC_RS_H
#define LRPTDEC_RS_H

#include "packet.h"

#define RS_N 255
#define RS_K 223
#define RS_2T (RS_N - RS_K)
#define RS_T (RS_2T / 2)
#define GEN_POLY 0x187  /* 110000111 */
#define FIRST_CONSEC_ROOT 112
#define DUAL_BASIS_BASE 117

typedef struct {
	int interleaving;
	uint8_t poly_zeroes[RS_2T];
} ReedSolomon;

void         rs_deinit(ReedSolomon *r);
ReedSolomon *rs_init(int interleaving);
int          rs_fix_packet(ReedSolomon *self, Cvcdu *c, int *corr_info);



#endif

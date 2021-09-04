#ifndef rs_h
#define rs_h

#include <stdint.h>
#include "protocol/vcdu.h"

#define RS_N 255
#define RS_K 223
#define RS_T (RS_N - RS_K)
#define RS_T2 (RS_T / 2)
#define GEN_POLY 0x187
#define FIRST_ROOT 112
#define ROOT_SKIP 11
#define INTERLEAVING 4

/**
 * Initialize the Reed-Solomon decoder
 */
void rs_init();

/**
 * Attempt to fix the data inside a VCDU.
 *
 * @param c the VCDU to error correct. If all errors can be corrected, bytes
 *          will be modified in-place.
 * @return -1  errors could not be corrected
 *         >=0 number of errors corrected. The VCDU argument can be assumed to
 *             be free of any errors in this case.
 */
int rs_fix(Vcdu *c);

#endif /* rs_h */

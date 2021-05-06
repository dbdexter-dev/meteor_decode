#include <stdint.h>
#include "jpeg/huffman.h"
#include "jpeg/jpeg.h"
#include "mcu_parser.h"
#include "protocol/mcu.h"

int
avhrr_decode(uint8_t (*dst)[8][8], AVHRR *a, int len)
{
	//const uint8_t quant_table = avhrr_quant_table(a); /* Unused by M2 */
	const uint8_t q_factor = avhrr_q(a);
	int16_t tmp[MCU_PER_MPDU][8][8];
	int i;

	if (!q_factor) return 1;

	/* Huffman decode, return on error */
	if (huffman_decode(tmp, a->data, MCU_PER_MPDU, len)) {
		return 1;
	}

	/* JPEG decode each 8x8 block */
	for (i=0; i<MCU_PER_MPDU; i++) {
		jpeg_decode(dst[i], tmp[i], q_factor);
	}

	return 0;
}

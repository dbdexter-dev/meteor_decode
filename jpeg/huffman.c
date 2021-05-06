#include <assert.h>
#include <stdint.h>
#include "huffman.h"
#include "utils.h"

#define EOB 0x00

static int get_dc_cat(uint16_t codeword);

static const uint8_t _dc_prefix_size[12] = {2, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9};
static const uint8_t _ac_table_size[17] = {0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125};
static const uint8_t _ac_table[] =
{
	1, 2,
	3,
	0, 4, 17,
	5, 18, 33,
	49, 65,
	6, 19, 81, 97,
	7, 34, 113,
	20, 50, 129, 145, 161,
	8, 35, 66, 177, 193,
	21, 82, 209, 240,
	36, 51, 98, 114,
	130,
	9, 10, 22, 23, 24, 25, 26, 37, 38, 39, 40, 41, 42, 52, 53, 54, 55, 56, 57,
	58, 67, 68, 69, 70, 71, 72, 73, 74, 83, 84, 85, 86, 87, 88, 89, 90, 99, 100,
	101, 102, 103, 104, 105, 106, 115, 116, 117, 118, 119, 120, 121, 122, 131,
	132, 133, 134, 135, 136, 137, 138, 146, 147, 148, 149, 150, 151, 152, 153,
	154, 162, 163, 164, 165, 166, 167, 168, 169, 170, 178, 179, 180, 181, 182,
	183, 184, 185, 186, 194, 195, 196, 197, 198, 199, 200, 201, 202, 210, 211,
	212, 213, 214, 215, 216, 217, 218, 225, 226, 227, 228, 229, 230, 231, 232,
	233, 234, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250
};

int
huffman_decode(int16_t (*dst)[8][8], const uint8_t *src, int count, int maxlen)
{
	int i, j, r, runlength;
	unsigned int bit_idx;
	uint32_t dc_info, ac_info, ac_buf;
	int dc_category;
	uint16_t dc_extra_bits;
	uint16_t ac_category, ac_extra_bits;
	uint16_t first_coeff;
	int dc_sign, dc_coeff;
	int ac_sign, ac_coeff;
	int ac_idx;
	int bytecount;

	bit_idx = 0;
	bytecount = 0;
	dc_coeff = 0;

	for (i=0; i<count; i++) {
		/* Decompress the DC coefficient */
		dc_info = read_bits(src, bit_idx, 32);
		if ((dc_category = get_dc_cat(dc_info>>16)) < 0) return 0;
		dc_sign = (dc_info >> (31 - _dc_prefix_size[dc_category])) & 0x1;
		dc_extra_bits = (dc_info >> (31 - _dc_prefix_size[dc_category] - dc_category + 1)) & ((1<<(dc_category-1))-1);

		/* Update the DC coefficient if the category is nonzero */
		if (dc_category) {
			dc_coeff += dc_extra_bits + (dc_sign ? (1<<(dc_category-1)) : 1-(1<<(dc_category)));
		}
		dst[i][0][0] = dc_coeff;

		bit_idx += _dc_prefix_size[dc_category] + dc_category;
		src += bit_idx/8;
		bytecount += bit_idx/8;
		bit_idx %= 8;

		/* Deal with corrupted data: sometimes a packet header is fine but the
		 * data contains a bunch of zeroes, which all code run=0 size=1 AC
		 * coefficients. This causes the Huffman decoder to consume all the
		 * bytes in the MPDU, and then keep going past the end of the buffer,
		 * unless this condition is checked. */
		if (bytecount >= maxlen) {
			break;
		}

		/* Decompress AC coefficients */
		for (r=1; r<64; r++) {
			ac_buf = read_bits(src, bit_idx, 32);

			/* Find the AC code in the AC Huffman table */
			first_coeff = 0;
			ac_idx = 0;
			for (j=2; j<(int)LEN(_ac_table_size); j++) {
				ac_info = ac_buf >> (32 - j);

				/* If the coefficient belongs to this range, decompress it */
				if (ac_info - first_coeff < _ac_table_size[j]) {
					ac_info = _ac_table[ac_idx + ac_info - first_coeff];
					break;
				}

				first_coeff = (first_coeff + _ac_table_size[j]) << 1;
				ac_idx += _ac_table_size[j];
			}
			bit_idx += j;

			if (ac_info == EOB) {
				for (; r<64; r++) {
					dst[i][r/8][r%8] = 0;
				}
			} else {
				/* Split the codeword into runlength and bit count */
				runlength = ac_info >> 4 & 0x0F;
				ac_category = ac_info & 0x0F;

				ac_sign = read_bits(src, bit_idx, 1);
				ac_extra_bits = read_bits(src, bit_idx+1, ac_category-1);

				if (ac_category > 0) {
					ac_coeff = ac_extra_bits + (ac_sign ? ((1<<(ac_category-1))) : 1-(1<<(ac_category)));
				} else {
					ac_coeff = 0;
				}

				/* Write runlength zeroes */
				for (; runlength>0 && r<64-1; runlength--, r++) {
					dst[i][r/8][r%8] = 0;
				}

				/* Write coefficient */
				dst[i][r/8][r%8] = ac_coeff;
				bit_idx += ac_category;
			}

			src += bit_idx/8;
			bytecount += bit_idx/8;
			bit_idx %= 8;
			if (bytecount >= maxlen) {
				break;
			}
		}

		if (bytecount >= maxlen) {
			break;
		}
	}

	//assert(bytecount < maxlen);

	return 0;
}

/* Static functions {{{ */
static int
get_dc_cat(uint16_t codeword)
{
	if (codeword >> 14 == 0) return 0;
	if (codeword >> 13 < 7) return (codeword >> 13) - 1;
	if (codeword >> 12 < 0xF) return 6;
	if (codeword >> 11 < 0x1F) return 7;
	if (codeword >> 10 < 0x3F) return 8;
	if (codeword >> 9 < 0x7F) return 9;
	if (codeword >> 8 < 0xFF) return 10;
	if (codeword >> 7 < 0x1FF) return 11;
	return -1;
}
/* }}} */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "huffman.h"
#include "packet.h"
#include "utils.h"

static void     init_ac_table();
static int      get_dc_cat(uint16_t codeword);
static int      get_ac_cat(uint16_t codeword);
static uint32_t get_bits(const uint8_t *ptr, int bit_offset, int nbits);

static uint8_t   _ac_table[65535];
static const int _cat_prefix_size[12] = {2, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9};
static const int _max_range[12] = {0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047};
static const int _min_range[12] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
static const int _compressed_table_counts[16] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125};
static const int _compressed_table[162] =
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

void
huffman_init()
{
	init_ac_table();
}

/* RLE and Huffman together, this is a monstrosity */
int
huffman_decode(int16_t (*dst)[8][8], const uint8_t *src, size_t count)
{
	int ac_cat, dc_cat;
	unsigned int runlength, ac_size;
	uint32_t dc_info, ac_info;
	int dc_signum, dc_coeff;
	int ac_signum, ac_val;
	size_t i, r;
	int byte_idx, bit_idx;

	bit_idx = 0;
	byte_idx = 0;
	dc_coeff = 0;
	for (i=0; i<count; i++) {
		/* Decompress the DC coefficient */
		dc_info = get_bits(src+byte_idx, bit_idx, 16);
		dc_cat = get_dc_cat(dc_info);

		/* If dc_cat is zero, dont update dc_coeff */
		if (dc_cat) {
			dc_signum = get_bits(src+byte_idx, bit_idx + _cat_prefix_size[dc_cat], 1);
			dc_coeff += get_bits(src+byte_idx, bit_idx + _cat_prefix_size[dc_cat]+1, dc_cat-1);

			/* signum = 0 -> lower range, signum = 1 -> upper range */
			if (dc_signum) {
				dc_coeff += _min_range[dc_cat];
			} else {
				dc_coeff -= _max_range[dc_cat];
			}
		}

		dst[i][0][0] = dc_coeff;
		bit_idx += _cat_prefix_size[dc_cat] + dc_cat;

		if (bit_idx > 8) {
			byte_idx += bit_idx/8;
			bit_idx %= 8;
		}

		/* Decompress the AC coefficients */
		for (r=1; r<64; r++) {
			ac_info = get_bits(src+byte_idx, bit_idx, 2);
			bit_idx += 2;

			/* Find the AC code in the AC Huffman table */
			while ((ac_cat = get_ac_cat(ac_info)) < 0) {
				/* Not a valid code yet: fetch another bit */
				ac_info = (ac_info << 1) | get_bits(src+byte_idx, bit_idx, 1);
				bit_idx++;
				if (ac_info > 65535) {
					return -1;
				}
			}

			/* EOB sequence, 4 bits long */
			if (ac_cat == 0x00) {
				for (; r<64; r++) {
					dst[i][r/8][r%8] = 0;
				}
			} else {
				/* Extract the runlength and the ac_size bits */
				runlength = (ac_cat >> 4) & 0x0F;
				ac_size = ac_cat & 0x0F;

				/* Extract the AC value */
				ac_signum = get_bits(src+byte_idx, bit_idx, 1);
				ac_val = get_bits(src+byte_idx, bit_idx, ac_size-1);

				if (ac_signum) {
					ac_val += _min_range[ac_size];
				} else {
					ac_val -= _max_range[ac_size];
				}

				/* Add the zeroes in */
				for(; runlength>0; runlength--) {
					dst[i][r/8][r%8] = 0;
					r++;
				}

				/* Append the extracted AC value */
				dst[i][r/8][r%8] = ac_val;
				bit_idx += ac_size;
			}

			if (bit_idx > 7) {
				byte_idx += bit_idx/8;
				bit_idx %= 8;
			}
		}
	}
	return byte_idx;
}

/* Static functions {{{ */
/* Get up to 32 misaligned bits from a buffer */
static uint32_t
get_bits(const uint8_t *ptr, int bit_offset, int nbits)
{
	int i;
	uint32_t ret, msk;

	msk = (1<<nbits)-1;

	i = 0;
	while (bit_offset > 7) {
		i++;
		bit_offset -= 8;
	}

	/* Read the first fragment from the first byte */
	ret = (ptr[i] & ((1<<(8-bit_offset))-1)) >> MAX(0, 8 - bit_offset - nbits);
	nbits -= 8-bit_offset;

	/* Append full bytes from the middle of the selected bit range */
	for (i++; i<=nbits/8; i++) {
		ret = (ret << 8) | ptr[i];
	}

	nbits -= (i-1)*8;
	/* Append the last fragment from the last byte */
	if (nbits > 0) {
		ret = (ret << (nbits%8)) | ((ptr[i] >> (8-nbits%8)) & ((1<<nbits)-1));
	}

	return ret & msk;
}

/* Initialize the AC Huffman lookup table */
static void
init_ac_table()
{
	int i, code, total_idx;
	int count;

	memset(_ac_table, 0, sizeof(_ac_table));
	code = 0;
	total_idx = 0;
	for (i=0; i<16; i++) {
		count = _compressed_table_counts[i];
		for (; count>0; count--) {
			_ac_table[code] = _compressed_table[total_idx++];
			code++;
		}
		code <<= 1;
	}
}

/* Search the Huffman tree for that codeword, return the decompressed value, or
 * -1 if the codeword isn't valid */
static int
get_ac_cat(uint16_t codeword)
{
	int tmp;

	/* EOB */
	if (codeword == 0xa) {
		return 0;
	}
	tmp = _ac_table[codeword];

	if (tmp == 0) {
		return -1;
	}

	return tmp;
}

/* Get DC category (aka number of bits used to represent the DC component) */
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
/*}}}*/

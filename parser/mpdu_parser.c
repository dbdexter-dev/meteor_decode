#include <stdint.h>
#include <string.h>
#include "mpdu_parser.h"

static enum {
	IDLE, HEADER, DATA
} _state;

static uint16_t _offset, _frag_offset;

void
mpdu_parser_init()
{
	_state = IDLE;
}

ParserStatus
mpdu_reconstruct(Mpdu *dst, Vcdu *src)
{
	unsigned int bytes_left;
	unsigned int jmp_idle;

	/* If a packet with a huge corrupted size makes it past the RS error
	 * checking, but the current VCDU is marked as containing a header pointer,
	 * jump to idle after decoding. This ensures that we don't lose tons of
	 * MPDUs because we think they are part of a huge MPDU that doesn't really
	 * exist */
	jmp_idle = (vcdu_header_present(src) && _offset == 0);

	/* If the VCDU contains known bad data, skip it completely */
	if (!vcdu_version(src) || !vcdu_type(src)) return PROCEED;

	switch (_state) {
		case IDLE:
			/* Get the pointer to the next header, set that as the new offset */
			if (vcdu_header_present(src)) {
				_offset = vcdu_header_ptr(src);

				/* Return immediately on invalid header pointer */
				if (_offset > VCDU_DATA_LENGTH) {
					return PROCEED;
				}

				_frag_offset = 0;
				_state = HEADER;
				return FRAGMENT;
			}
			return PROCEED;
			break;
		case HEADER:
			bytes_left = MPDU_HDR_LEN - _frag_offset;

			if (_offset + bytes_left < VCDU_DATA_LENGTH) {
				/* The header's end byte is contained in this VCDU: copy bytes */
				memcpy((uint8_t*)dst + _frag_offset, src->mpdu_data + _offset, bytes_left);
				_frag_offset = 0;       /* 0 bytes into the data fragment */
				_offset += bytes_left;
				_state = DATA;
				return FRAGMENT;
			}

			/* The header's end byte is in the next VCDU: copy some bytes and
			 * update the fragment offset */
			memcpy((uint8_t*)dst + _frag_offset, src->mpdu_data + _offset, VCDU_DATA_LENGTH - _offset);
			_frag_offset += VCDU_DATA_LENGTH - _offset;
			_offset = 0;
			return PROCEED;
			break;
		case DATA:
			bytes_left = mpdu_len(dst) - _frag_offset;

			if (_offset + bytes_left < VCDU_DATA_LENGTH) {
				/* The end of this data segment is within the VCDU: copy bytes */
				memcpy((uint8_t*)(&dst->data) + _frag_offset, src->mpdu_data + _offset, bytes_left);
				_frag_offset = 0;
				_offset += bytes_left;
				_state = jmp_idle ? IDLE : HEADER;
				return PARSED;
			}

			/* The data continues in the next VCDU: copy some bytes and update
			 * the fragment offset */
			memcpy((uint8_t*)(&dst->data) + _frag_offset, src->mpdu_data + _offset, VCDU_DATA_LENGTH - _offset);
			_frag_offset += VCDU_DATA_LENGTH - _offset;
			_offset = 0;
			_state = jmp_idle ? IDLE : DATA;
			return jmp_idle ? FRAGMENT : PROCEED;
			break;
	}

	return PROCEED;
}

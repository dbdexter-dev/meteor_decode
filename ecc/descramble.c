#include <stdint.h>
#include "descramble.h"
#include "utils.h"

static uint8_t _noise[NOISE_PERIOD];

void
descramble_init()
{
	int i, j;
	uint8_t state, newbit;

	state = 0xFF;
	for (i=0; i<NOISE_PERIOD; i++) {
		_noise[i] = 0;
		for (j=0; j<8; j++) {
			newbit = (state >> 7 & 0x1) ^ (state >> 5 & 0x1) ^ (state >> 3 & 0x1) ^
					 (state >> 0 & 0x1);

			_noise[i] = _noise[i] << 1 | (state & 0x1);
			state = (state >> 1) | (newbit << 7);
		}
	}
}

void
descramble(Cadu *c)
{
	int i;
	uint8_t *const cadu_data = (uint8_t*)(&c->data);

	for (i=0; i<CADU_DATA_LENGTH; i++) {
		cadu_data[i] ^= _noise[i % NOISE_PERIOD];
	}
}

#ifndef LRPTDEC_VITERBI_H
#define LRPTDEC_VITERBI_H

#include <stdint.h>
#include "file.h"

#define K 7
#define G1 0x79
#define G2 0x5B

#define N_STATES (1 << K)
#define MEM_DEPTH (5 * N_STATES)
#define MAX_COST (MEM_DEPTH * 512)
#define BACKTRACK_DEPTH ((MEM_DEPTH - N_STATES) & 0xFFFFFFF8)

typedef struct {
	uint8_t output;
	unsigned int next_state;
} Transition;


typedef struct state {
	int id;
	unsigned int cost;
	uint8_t data[MEM_DEPTH+1];
	int depth;
} Path;

typedef struct {
	Transition trans[N_STATES][2];
	Path *mem[N_STATES];
	Path *tmp[N_STATES];
} Viterbi;


void     viterbi_deinit(Viterbi *v);
int      viterbi_decode(Viterbi *v, const int8_t *in, size_t len, uint8_t *out);
int      viterbi_encode(const uint8_t *in, size_t len, uint8_t *out);
Viterbi* viterbi_init();

#endif

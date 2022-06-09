#ifndef _PERFT_H_
#define _PERFT_H_

#include "move.h"

uint64_t perft(Position * pos , int depth);
void perft_test(Position* pos , int depth);

#endif
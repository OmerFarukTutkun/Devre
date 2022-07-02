#ifndef _HISTORY_H_
#define _HISTORY_H_
#include "movegen.h"

void update_histories(Position* pos, int depth, uint16_t* moves, int length);
int16_t get_history(Position* pos, uint16_t move);
#endif

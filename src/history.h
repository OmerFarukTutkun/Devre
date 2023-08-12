#ifndef DEVRE_HISTORY_H
#define DEVRE_HISTORY_H

#include "types.h"
#include "search.h"
#include "Thread.h"
#include "move.h"

void updateHistories(Thread &thread, Stack *ss, int depth, MoveList &movelist, uint16_t bestmove);

int getCaptureHistory(Thread &thread, Stack *ss, uint16_t move);

int getQuietHistory(Thread &thread, Stack *ss, uint16_t move);

#endif //DEVRE_HISTORY_H

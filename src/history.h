#ifndef DEVRE_HISTORY_H
#define DEVRE_HISTORY_H

#include "types.h"
#include "search.h"
#include "ThreadData.h"
#include "move.h"

void updateHistories(ThreadData &thread, Stack *ss, int depth, MoveList &movelist, uint16_t bestmove);

int getCaptureHistory(ThreadData &thread, Stack *ss, uint16_t move);

int getQuietHistory(ThreadData &thread, Stack *ss, uint16_t move);

#endif //DEVRE_HISTORY_H

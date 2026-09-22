#ifndef DEVRE_HISTORY_H
#define DEVRE_HISTORY_H

#include "types.h"
#include "search.h"
#include "threadData.h"
#include "move.h"

void updateHistories(ThreadData& thread, Stack* ss, int depth, uint16_t bestmove);

void updatePrevMoveFailLowBonus(ThreadData& thread, Stack* ss, int depth);

int getCaptureHistory(ThreadData& thread, Stack* ss, uint16_t move);

int getQuietHistory(ThreadData& thread, Stack* ss, uint16_t move);

int getContHistory(ThreadData& thread, Stack* ss, uint16_t move);

int getReductionRegret(ThreadData& thread, uint16_t move);

void updateReductionRegret(ThreadData& thread, uint16_t move, int depth, bool regret);


void updateCorrHistScore(ThreadData& thread, Stack* ss, int depth, int diff);

int adjustEvalWithCorrHist(ThreadData& thread, Stack* ss, int rawEval);

#endif  //DEVRE_HISTORY_H

#include "history.h"
#include "tuning.h"
#include "nnue.h"

const int HistoryDivisor = 16384;

int statBonus(int depth) {
    return std::min(400 * depth -100, 1500);
}

void updateHistory(int16_t *current, int depth, bool good) {

    const int delta = good ? statBonus(depth) : -statBonus(depth);
    *current += delta - *current * std::abs(delta) / HistoryDivisor;
}

void updateQuietHistories(ThreadData &thread, Stack *ss, int depth, uint16_t bestMove) {
    if ((ss->played == 1 && depth <= 3))
        return;

    Board *board = &thread.board;
    thread.counterMoves[board->sideToMove][moveFrom((ss - 1)->move)][moveTo((ss - 1)->move)] = bestMove;

    if (ss->killers[0] != bestMove) {
        ss->killers[1] = ss->killers[0];
        ss->killers[0] = bestMove;
    }
    for (int i = 0; i <ss->played; i++) {
        uint16_t move = ss->playedMoves[i];
        if (isQuiet(move)) {
            int from = moveFrom(move);
            int to = moveTo(move);
            int piece = board->pieceBoard[from];
            bool isGood = (i ==ss->played - 1);

            int16_t* current = &thread.history[checkBit(ss->threat, from)][checkBit(ss->threat, to)][board->sideToMove][from][to];
            updateHistory(current, depth, isGood);
            
            if ((ss - 1)->move)
            {
                current = &(*(ss - 1)->continuationHistory)[piece][to];
                updateHistory(current, depth, isGood);
            }
            if ((ss - 2)->move)
            {
                current = &(*(ss - 2)->continuationHistory)[piece][to];
                updateHistory(current, depth, isGood);
            }
        }
    }
}

void updateCaptureHistories(ThreadData &thread, Stack *ss, int depth) {
    Board *board = &thread.board;
    for (int i = 0; i <ss->played; i++) {
        auto move = ss->playedMoves[i];
        if (moveType(move) == CAPTURE) {
            int from = moveFrom(move);
            int to = moveTo(move);
            int16_t *current = &thread.captureHist[board->sideToMove][pieceType(board->pieceBoard[from])][to][pieceType(
                    board->pieceBoard[to])];
            updateHistory(current, depth, i == ss->played - 1);
        }
    }
}

void updateHistories(ThreadData &thread, Stack *ss, int depth, uint16_t bestMove) {
    if (isQuiet(moveType(bestMove)))
        updateQuietHistories(thread, ss, depth, bestMove);

    if (moveType(bestMove) <= CAPTURE)
        updateCaptureHistories(thread, ss, depth);
}

int getCaptureHistory(ThreadData &thread, Stack *ss, uint16_t move) {
    Board *board = &thread.board;
    int from = moveFrom(move);
    int to = moveTo(move);
    return thread.captureHist[board->sideToMove][pieceType(board->pieceBoard[from])][to][pieceType(board->pieceBoard[to])];
}

int getQuietHistory(ThreadData &thread, Stack *ss, uint16_t move) {
    Board *board = &thread.board;
    int from = moveFrom(move);
    int to = moveTo(move);
    int piece = board->pieceBoard[from];
    int score = thread.history[checkBit(ss->threat, from)][checkBit(ss->threat, to)][board->sideToMove][from][to];


    if ((ss - 1)->move)
    {
        score += (*(ss - 1)->continuationHistory)[piece][to];
    }
    if ((ss - 2)->move)
    {

        score += (*(ss - 2)->continuationHistory)[piece][to];
    }
    return score;
}


int getContHistory(ThreadData &thread, Stack *ss, uint16_t move)
{
    Board *board = &thread.board;
    int from = moveFrom(move);
    int to = moveTo(move);
    int piece = board->pieceBoard[from];
    auto score = 0;

    if ((ss - 1)->move)
    {
        score += (*(ss - 1)->continuationHistory)[piece][to];
    }
    if ((ss - 2)->move)
    {

        score += (*(ss - 2)->continuationHistory)[piece][to];
    }
    return score;
}
void updateCorrHistScore(ThreadData &thread, Stack *ss, const int depth, const int diff) {

    auto * board = &thread.board;

    int to = moveTo((ss-1)->move);
    int piece = board->pieceBoard[to];

    //it will be true for null move
    if(piece == EMPTY)
        piece = PAWN;

    auto & contcorrHistEntry = (*(ss - 2)->contCorrHist)[piece][to];

    int &pawnCorrHistEntry = thread.corrHist[board->sideToMove][board->pawnKey % 16384][0];
    int &nonPawnCorrHistEntryWhite = thread.corrHist[board->sideToMove][board->nonPawnKey[WHITE] % 16384][1];
    int &nonPawnCorrHistEntryBlack = thread.corrHist[board->sideToMove][board->nonPawnKey[BLACK] % 16384][2];

    const int bonus = diff*depth/8;
    const int D = 1024;
    int clampedBonus = std::clamp(bonus, -D, D);
    
    pawnCorrHistEntry += clampedBonus - pawnCorrHistEntry * std::abs(clampedBonus) / D;
    nonPawnCorrHistEntryWhite += clampedBonus - nonPawnCorrHistEntryWhite * std::abs(clampedBonus) / D;
    nonPawnCorrHistEntryBlack += clampedBonus - nonPawnCorrHistEntryBlack * std::abs(clampedBonus) / D;
    contcorrHistEntry += clampedBonus - contcorrHistEntry * std::abs(clampedBonus) / D;
}

int adjustEvalWithCorrHist(ThreadData &thread,Stack *ss, const int rawEval) {
    auto * board = &thread.board;

    int &pawnCorrHistEntry = thread.corrHist[board->sideToMove][board->pawnKey % 16384][0];
    int &nonPawnCorrHistEntryWhite = thread.corrHist[board->sideToMove][board->nonPawnKey[WHITE] % 16384][1];
    int &nonPawnCorrHistEntryBlack = thread.corrHist[board->sideToMove][board->nonPawnKey[BLACK] % 16384][2];

    int to = moveTo((ss-1)->move);
    int piece = board->pieceBoard[to];

    //it will be true for null move
    if(piece == EMPTY)
        piece = PAWN;

    auto & contcorrHistEntry = (*(ss - 2)->contCorrHist)[piece][to];

    const int average = (51*pawnCorrHistEntry + 51*nonPawnCorrHistEntryWhite + 51*nonPawnCorrHistEntryBlack + contcorrHistEntry*51)/ 512;

    auto eval = rawEval + average;
    eval = eval*NNUE::Instance()->halfMoveScale(thread.board)*NNUE::Instance()->materialScale(thread.board);
    return std::clamp(eval , -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

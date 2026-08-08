#include "history.h"
#include "tuning.h"
#include "nnue.h"

DEFINE_PARAM_B(histDivisor, 16384, 8000, 26000);
DEFINE_PARAM_B(histBonusScale, 400, 180, 620);
DEFINE_PARAM_B(histBonusOffset, 100, 0, 300);
DEFINE_PARAM_B(histBonusMax, 1500, 700, 2400);

// Correction history. The 512 denominator is left fixed: scaling every
// numerator is the same move, so tuning it too would be a redundant direction.
DEFINE_PARAM_B(corrWeightPawn, 52, 0, 128);
DEFINE_PARAM_B(corrWeightNonPawnW, 52, 0, 128);
DEFINE_PARAM_B(corrWeightNonPawnB, 52, 0, 128);
DEFINE_PARAM_B(corrWeightCont, 47, 0, 128);
DEFINE_PARAM_B(corrWeightThreat, 37, 0, 128);
DEFINE_PARAM_B(corrWeightMajor, 30, 0, 128);
DEFINE_PARAM_B(corrHistD, 1024, 512, 2048);
DEFINE_PARAM_B(corrBonusDiv, 8, 2, 20);

int statBonus(int depth) { return std::min<int>(histBonusScale * depth - histBonusOffset, histBonusMax); }

void updateHistory(int16_t* current, int depth, bool good) {

    const int delta = good ? statBonus(depth) : -statBonus(depth);
    *current += delta - *current * std::abs(delta) / histDivisor;
}
// When a node fails low, the opponent's previous quiet move "worked":
// give it a continuation-history bonus.
void updatePrevMoveFailLowBonus(ThreadData& thread, Stack* ss, int depth) {
    Board*   board = &thread.board;
    uint16_t prev  = (ss - 1)->move;

    if (prev == NO_MOVE || prev == NULL_MOVE || !isQuiet(prev))
        return;

    int to    = moveTo(prev);
    int piece = board->pieceBoard[to];

    if ((ss - 2)->move)
        updateHistory(&(*(ss - 2)->continuationHistory)[piece][to], depth, true);
}

void updateQuietHistories(ThreadData& thread, Stack* ss, int depth, uint16_t bestMove) {
    if ((ss->played == 1 && depth <= 3))
        return;

    Board* board                                                                             = &thread.board;
    thread.counterMoves[board->sideToMove][moveFrom((ss - 1)->move)][moveTo((ss - 1)->move)] = bestMove;

    if (ss->killers[0] != bestMove)
    {
        ss->killers[1] = ss->killers[0];
        ss->killers[0] = bestMove;
    }
    int pawnBucket = board->pawnKey % ThreadData::PAWN_HIST_SIZE;
    for (int i = 0; i < ss->played; i++)
    {
        uint16_t move = ss->playedMoves[i];
        if (isQuiet(move))
        {
            int  from   = moveFrom(move);
            int  to     = moveTo(move);
            int  piece  = board->pieceBoard[from];
            bool isGood = (i == ss->played - 1);

            int16_t* current = &thread.history[checkBit(ss->threat, from)][checkBit(ss->threat, to)][board->sideToMove][from][to];
            updateHistory(current, depth, isGood);

            // pawn history
            current = &thread.pawnHistory[pawnBucket][piece][to];
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

void updateCaptureHistories(ThreadData& thread, Stack* ss, int depth) {
    Board* board = &thread.board;
    for (int i = 0; i < ss->played; i++)
    {
        auto move = ss->playedMoves[i];
        if (moveType(move) == CAPTURE)
        {
            int      from    = moveFrom(move);
            int      to      = moveTo(move);
            int16_t* current = &thread.captureHist[board->sideToMove][pieceType(board->pieceBoard[from])][to][pieceType(board->pieceBoard[to])];
            updateHistory(current, depth, i == ss->played - 1);
        }
    }
}

void updateHistories(ThreadData& thread, Stack* ss, int depth, uint16_t bestMove) {
    if (isQuiet(moveType(bestMove)))
        updateQuietHistories(thread, ss, depth, bestMove);

    if (moveType(bestMove) <= CAPTURE)
        updateCaptureHistories(thread, ss, depth);
}

int getCaptureHistory(ThreadData& thread, Stack* ss, uint16_t move) {
    Board* board = &thread.board;
    int    from  = moveFrom(move);
    int    to    = moveTo(move);
    return thread.captureHist[board->sideToMove][pieceType(board->pieceBoard[from])][to][pieceType(board->pieceBoard[to])];
}

int getQuietHistory(ThreadData& thread, Stack* ss, uint16_t move) {
    Board* board = &thread.board;
    int    from  = moveFrom(move);
    int    to    = moveTo(move);
    int    piece = board->pieceBoard[from];
    int    score = thread.history[checkBit(ss->threat, from)][checkBit(ss->threat, to)][board->sideToMove][from][to];

    // pawn history
    int pawnBucket = board->pawnKey % ThreadData::PAWN_HIST_SIZE;
    score += thread.pawnHistory[pawnBucket][piece][to];

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


int getContHistory(ThreadData& thread, Stack* ss, uint16_t move) {
    Board* board = &thread.board;
    int    from  = moveFrom(move);
    int    to    = moveTo(move);
    int    piece = board->pieceBoard[from];
    auto   score = 0;

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

void updateCorrHistScore(ThreadData& thread, Stack* ss, const int depth, const int diff) {

    auto* board = &thread.board;


    bool isMoveOk = (ss - 1)->move != NO_MOVE && (ss - 1)->move != NULL_MOVE;


    int& pawnCorrHistEntry         = thread.corrHist[board->sideToMove][board->pawnKey % 16384][0];
    int& nonPawnCorrHistEntryWhite = thread.corrHist[board->sideToMove][board->nonPawnKey[WHITE] % 16384][1];
    int& nonPawnCorrHistEntryBlack = thread.corrHist[board->sideToMove][board->nonPawnKey[BLACK] % 16384][2];
    int& majorCorrHistEntry        = thread.corrHist[board->sideToMove][board->majorKey % 16384][3];

    const int bonus        = diff * depth / corrBonusDiv;
    const int D            = corrHistD;
    int       clampedBonus = std::clamp(bonus, -D, D);

    pawnCorrHistEntry += clampedBonus - pawnCorrHistEntry * std::abs(clampedBonus) / D;
    nonPawnCorrHistEntryWhite += clampedBonus - nonPawnCorrHistEntryWhite * std::abs(clampedBonus) / D;
    nonPawnCorrHistEntryBlack += clampedBonus - nonPawnCorrHistEntryBlack * std::abs(clampedBonus) / D;
    majorCorrHistEntry += clampedBonus - majorCorrHistEntry * std::abs(clampedBonus) / D;

    if (isMoveOk)
    {
        int from  = moveFrom((ss - 1)->move);
        int to    = moveTo((ss - 1)->move);
        int piece = board->pieceBoard[to];

        auto& contcorrHistEntry           = (*(ss - 2)->contCorrHist)[piece][to];
        auto& contcorrHistEntryPly3       = (*(ss - 3)->contCorrHist)[piece][to];
        auto& threatLastMoveCorrHistEntry = thread.threatLastMoveCorrHist[checkBit((ss - 1)->threat, from)][checkBit((ss - 1)->threat, to)][board->sideToMove][from][to];

        contcorrHistEntry += clampedBonus - contcorrHistEntry * std::abs(clampedBonus) / D;
        contcorrHistEntryPly3 += clampedBonus - contcorrHistEntryPly3 * std::abs(clampedBonus) / D;

        threatLastMoveCorrHistEntry += clampedBonus - threatLastMoveCorrHistEntry * std::abs(clampedBonus) / D;
    }
}

int adjustEvalWithCorrHist(ThreadData& thread, Stack* ss, const int rawEval) {
    auto* board = &thread.board;

    int& pawnCorrHistEntry         = thread.corrHist[board->sideToMove][board->pawnKey % 16384][0];
    int& nonPawnCorrHistEntryWhite = thread.corrHist[board->sideToMove][board->nonPawnKey[WHITE] % 16384][1];
    int& nonPawnCorrHistEntryBlack = thread.corrHist[board->sideToMove][board->nonPawnKey[BLACK] % 16384][2];
    int  majorCorrHistEntry        = thread.corrHist[board->sideToMove][board->majorKey % 16384][3];

    bool isMoveOk = (ss - 1)->move != NO_MOVE && (ss - 1)->move != NULL_MOVE;

    auto contcorrHistEntry           = 0;
    auto threatLastMoveCorrHistEntry = 0;

    if (isMoveOk)
    {
        int from  = moveFrom((ss - 1)->move);
        int to    = moveTo((ss - 1)->move);
        int piece = board->pieceBoard[to];

        contcorrHistEntry = (*(ss - 2)->contCorrHist)[piece][to];
        contcorrHistEntry += (*(ss - 3)->contCorrHist)[piece][to];
        threatLastMoveCorrHistEntry = thread.threatLastMoveCorrHist[checkBit((ss - 1)->threat, from)][checkBit((ss - 1)->threat, to)][board->sideToMove][from][to];
    }

    const int average = (corrWeightPawn * pawnCorrHistEntry + corrWeightNonPawnW * nonPawnCorrHistEntryWhite + corrWeightNonPawnB * nonPawnCorrHistEntryBlack
                         + contcorrHistEntry * corrWeightCont + threatLastMoveCorrHistEntry * corrWeightThreat + majorCorrHistEntry * corrWeightMajor)
                      / 512;

    auto eval = rawEval + average;
    eval      = eval * NNUE::halfMoveScale(thread.board) * NNUE::materialScale(thread.board);
    return std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

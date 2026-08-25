#include "history.h"
#include "tuning.h"
#include "nnue.h"

DEFINE_PARAM_B(historyDivisor, 16728, 8000, 26000);

DEFINE_PARAM_B(quietHistoryBonusScale, 488, 200, 700);
DEFINE_PARAM_B(quietHistoryBonusOffset, -73, -200, 0);
DEFINE_PARAM_B(quietHistoryBonusMax, 1367, 700, 2200);
DEFINE_PARAM_B(quietHistoryMalusScale, 488, 200, 700);
DEFINE_PARAM_B(quietHistoryMalusOffset, -73, -200, 0);
DEFINE_PARAM_B(quietHistoryMalusMax, 1367, 700, 2200);

DEFINE_PARAM_B(pawnHistoryBonusScale, 488, 200, 700);
DEFINE_PARAM_B(pawnHistoryBonusOffset, -73, -200, 0);
DEFINE_PARAM_B(pawnHistoryBonusMax, 1367, 700, 2200);
DEFINE_PARAM_B(pawnHistoryMalusScale, 488, 200, 700);
DEFINE_PARAM_B(pawnHistoryMalusOffset, -73, -200, 0);
DEFINE_PARAM_B(pawnHistoryMalusMax, 1367, 700, 2200);

DEFINE_PARAM_B(captureHistoryBonusScale, 488, 200, 700);
DEFINE_PARAM_B(captureHistoryBonusOffset, -73, -200, 0);
DEFINE_PARAM_B(captureHistoryBonusMax, 1367, 700, 2200);
DEFINE_PARAM_B(captureHistoryMalusScale, 488, 200, 700);
DEFINE_PARAM_B(captureHistoryMalusOffset, -73, -200, 0);
DEFINE_PARAM_B(captureHistoryMalusMax, 1367, 700, 2200);

DEFINE_PARAM_B(contHistoryBonusScale, 488, 200, 700);
DEFINE_PARAM_B(contHistoryBonusOffset, -73, -200, 0);
DEFINE_PARAM_B(contHistoryBonusMax, 1367, 700, 2200);
DEFINE_PARAM_B(contHistoryMalusScale, 488, 200, 700);
DEFINE_PARAM_B(contHistoryMalusOffset, -73, -200, 0);
DEFINE_PARAM_B(contHistoryMalusMax, 1367, 700, 2200);

DEFINE_PARAM_B(corrHistBonusDepthDiv, 8, 1, 16);
DEFINE_PARAM_B(corrHistClamp, 1031, 400, 2000);
DEFINE_PARAM_B(corrHistPawnWeight, 54, 0, 150);
DEFINE_PARAM_B(corrHistNonPawnWhiteWeight, 55, 0, 150);
DEFINE_PARAM_B(corrHistNonPawnBlackWeight, 73, 0, 150);
DEFINE_PARAM_B(corrHistContWeight, 67, 0, 150);
DEFINE_PARAM_B(corrHistThreatWeight, 42, 0, 150);
DEFINE_PARAM_B(corrHistMajorWeight, 38, 0, 150);

int historyValue(int scale, int offset, int max, int depth) {
    return std::min(scale * depth + offset, max);
}

int quietHistoryBonus(int depth) { return historyValue(quietHistoryBonusScale, quietHistoryBonusOffset, quietHistoryBonusMax, depth); }
int quietHistoryMalus(int depth) { return historyValue(quietHistoryMalusScale, quietHistoryMalusOffset, quietHistoryMalusMax, depth); }
int pawnHistoryBonus(int depth) { return historyValue(pawnHistoryBonusScale, pawnHistoryBonusOffset, pawnHistoryBonusMax, depth); }
int pawnHistoryMalus(int depth) { return historyValue(pawnHistoryMalusScale, pawnHistoryMalusOffset, pawnHistoryMalusMax, depth); }
int captureHistoryBonus(int depth) { return historyValue(captureHistoryBonusScale, captureHistoryBonusOffset, captureHistoryBonusMax, depth); }
int captureHistoryMalus(int depth) { return historyValue(captureHistoryMalusScale, captureHistoryMalusOffset, captureHistoryMalusMax, depth); }
int contHistoryBonus(int depth) { return historyValue(contHistoryBonusScale, contHistoryBonusOffset, contHistoryBonusMax, depth); }
int contHistoryMalus(int depth) { return historyValue(contHistoryMalusScale, contHistoryMalusOffset, contHistoryMalusMax, depth); }

void updateHistory(int16_t* current, bool good, int bonus, int malus) {

    const int delta = good ? bonus : -malus;
    *current += delta - *current * std::abs(delta) / historyDivisor;
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
        updateHistory(&(*(ss - 2)->continuationHistory)[piece][to], true, contHistoryBonus(depth), contHistoryMalus(depth));
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
    const int quietBonus = quietHistoryBonus(depth);
    const int quietMalus = quietHistoryMalus(depth);
    const int pawnBonus  = pawnHistoryBonus(depth);
    const int pawnMalus  = pawnHistoryMalus(depth);
    const int contBonus  = contHistoryBonus(depth);
    const int contMalus  = contHistoryMalus(depth);
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
            updateHistory(current, isGood, quietBonus, quietMalus);

            // pawn history
            current = &thread.pawnHistory[pawnBucket][piece][to];
            updateHistory(current, isGood, pawnBonus, pawnMalus);

            if ((ss - 1)->move)
            {
                current = &(*(ss - 1)->continuationHistory)[piece][to];
                updateHistory(current, isGood, contBonus, contMalus);
            }
            if ((ss - 2)->move)
            {
                current = &(*(ss - 2)->continuationHistory)[piece][to];
                updateHistory(current, isGood, contBonus, contMalus);
            }
        }
    }
}

void updateCaptureHistories(ThreadData& thread, Stack* ss, int depth) {
    Board* board = &thread.board;
    const int captureBonus = captureHistoryBonus(depth);
    const int captureMalus  = captureHistoryMalus(depth);
    for (int i = 0; i < ss->played; i++)
    {
        auto move = ss->playedMoves[i];
        if (moveType(move) == CAPTURE)
        {
            int      from    = moveFrom(move);
            int      to      = moveTo(move);
            int16_t* current = &thread.captureHist[board->sideToMove][pieceType(board->pieceBoard[from])][to][pieceType(board->pieceBoard[to])];
            updateHistory(current, i == ss->played - 1, captureBonus, captureMalus);
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

    const int bonus        = diff * depth / corrHistBonusDepthDiv;
    const int D            = corrHistClamp;
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

    const int average =
      (corrHistPawnWeight * pawnCorrHistEntry + corrHistNonPawnWhiteWeight * nonPawnCorrHistEntryWhite + corrHistNonPawnBlackWeight * nonPawnCorrHistEntryBlack + contcorrHistEntry * corrHistContWeight + threatLastMoveCorrHistEntry * corrHistThreatWeight + majorCorrHistEntry * corrHistMajorWeight) / 512;

    auto eval = rawEval + average;
    eval      = eval * NNUE::halfMoveScale(thread.board) * NNUE::materialScale(thread.board);
    return std::clamp(eval, -MIN_MATE_SCORE + 1, MIN_MATE_SCORE - 1);
}

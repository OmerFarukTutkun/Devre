#ifndef DEVRE_THREADDATA_H
#define DEVRE_THREADDATA_H

#include "types.h"
#include "board.h"
class ThreadData {
public:
    int ThreadID;
    uint64_t nodes{0};
    int searchDepth{0};

    int16_t history[2][2][2][64][64]{0};   // history table for ordering quiet moves
    int16_t contHist[N_PIECES][N_SQUARES][N_PIECES][N_SQUARES]{0};
    int16_t captureHist[2][6][64][6]{0};
    uint16_t counterMoves[2][64][64]{0}; // a counter move for move ordering

    int corrHist[2][16384][3]{0};
    int16_t contCorrHist[N_PIECES][N_SQUARES][N_PIECES][N_SQUARES]{0};

    Board board;

    explicit ThreadData(const std::string &fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", int ID = 0);
    explicit ThreadData(Board& b, int ID = 0);

    virtual ~ThreadData();
};
#endif //DEVRE_THREADDATA_H

#ifndef DEVRE_THREAD_H
#define DEVRE_THREAD_H

#include "types.h"
#include "board.h"

#include <thread>
class Thread {
public:
    int ThreadID;
    uint64_t nodes{};

    int16_t history[2][2][2][64][64]{};   // history table for ordering quiet moves
    int16_t contHist[N_PIECES][N_SQUARES][N_PIECES][N_SQUARES]{};
    int16_t captureHist[2][6][64][6]{};
    uint16_t counterMoves[2][64][64]{}; // a counter move for move ordering

    Board board{};

    explicit Thread(const std::string &fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", int ID = 0);
    explicit Thread(const Board& b, int ID = 0);
};
#endif //DEVRE_THREAD_H

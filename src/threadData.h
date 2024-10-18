#ifndef DEVRE_THREADDATA_H
#define DEVRE_THREADDATA_H

#include "types.h"
#include "board.h"

class ThreadData {
   public:
    int      ThreadID;
    uint64_t nodes{0};
    uint64_t tbHits{0};
    int      searchDepth{0};

    int16_t  history[2][2][N_COLORS][N_SQUARES][N_SQUARES]{0};  // history table for ordering quiet moves
    int16_t  contHist[N_PIECES][N_SQUARES][N_PIECES][N_SQUARES]{0};
    int16_t  captureHist[N_COLORS][N_PIECE_TYPES][N_SQUARES][N_PIECE_TYPES]{0};
    int16_t  pawnHist[512][N_PIECES][N_SQUARES]{0};
    uint16_t counterMoves[N_COLORS][N_SQUARES][N_SQUARES]{0};  // a counter move for move ordering

    int     corrHist[N_COLORS][16384][4]{0};
    int16_t contCorrHist[N_PIECES][N_SQUARES][N_PIECES][N_SQUARES]{0};
    int16_t threatLastMoveCorrHist[N_COLORS][2][2][N_SQUARES][N_SQUARES]{0};
    Board   board;

    explicit ThreadData(
      const std::string& fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      int                ID  = 0);

    explicit ThreadData(Board& b, int ID = 0);

    virtual ~ThreadData();
};

#endif  //DEVRE_THREADDATA_H

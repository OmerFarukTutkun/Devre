#ifndef DEVRE_BOARD_H
#define DEVRE_BOARD_H

#include "bitboard.h"
#include "types.h"

class Board {
public:
    explicit Board(const std::string &fen = START_FEN);

    void print();

    std::string getFen();

    void addPiece(int piece, int sq);

    void removePiece(int piece, int sq);

    void movePiece(int piece, int from, int to);

    int eval();

    void makeMove(uint16_t move, bool updateNNUE = true);

    void unmakeMove(uint16_t move, bool updateNNUE = true);

    void makeNullMove();

    void unmakeNullMove();

    bool hasNonPawnPieces();

    bool isDraw();

    bool isMaterialDraw();

    bool isRepetition();

    bool inCheck();

    bool inCheck(uint64_t threat);

    uint64_t threat();

    uint64_t key;
    uint16_t nnueKey;
    uint64_t pawnKey;
    uint64_t nonPawnKey[2];
    uint64_t bitboards[N_PIECES];
    uint64_t occupied[N_COLORS];
    uint8_t pieceBoard[N_SQUARES];

    uint8_t castlings, halfMove;
    Color sideToMove;
    uint8_t castlingRooks[4];
    uint8_t enPassant;
    uint16_t fullMove;


    std::vector<BoardHistory> boardHistory;
    NNUEData nnueData;
};

#endif //DEVRE_BOARD_H

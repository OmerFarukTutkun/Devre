#ifndef DEVRE_ZOBRIST_H
#define DEVRE_ZOBRIST_H


#include "types.h"

class Zobrist {
private:
    Zobrist();

public:
    static Zobrist *Instance();

    uint64_t PieceKeys[N_PIECES][64];
    uint64_t SideToPlayKey;
    uint64_t CastlingKeys[16];
    uint64_t EnPassantKeys[NO_SQ + 1];
};


#endif //DEVRE_ZOBRIST_H

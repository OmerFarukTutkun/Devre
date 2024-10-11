#include "zobrist.h"

uint64_t rand_u64() {
    //use stockfish prng
    static uint64_t s = 1070372;
    s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
    return s * 2685821657736338717LL;
}

Zobrist::Zobrist() {
    for (auto &PieceKey: PieceKeys) {
        for (auto &j: PieceKey) {
            j = rand_u64();
        }
    }
    SideToPlayKey = rand_u64();
    for (auto &CastlingKey: CastlingKeys)
        CastlingKey = rand_u64();

    for (auto &EnPassantKey: EnPassantKeys)
        EnPassantKey = rand_u64();
}

Zobrist *Zobrist::Instance() {
    static auto instance = Zobrist();
    return &instance;
}

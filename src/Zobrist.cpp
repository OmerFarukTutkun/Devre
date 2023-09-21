#include "Zobrist.h"

uint64_t rand_u64()
{
    static uint64_t seed= 1442695040888963407;
    const uint64_t a = 6364136223846793005;
    seed *= a;
    return seed;
}

Zobrist::Zobrist() {
    for(auto & PieceKey : PieceKeys)
    {
        for(auto & j : PieceKey)
        {
            j = rand_u64();
        }
    }
    SideToPlayKey = rand_u64();
    for(auto & CastlingKey : CastlingKeys)
        CastlingKey = rand_u64();

    for(auto & EnPassantKey : EnPassantKeys)
        EnPassantKey = rand_u64();
}

Zobrist *Zobrist::Instance() {
    static auto instance = Zobrist();
    return &instance;
}

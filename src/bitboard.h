#ifndef DEVRE_BITBOARD_H
#define DEVRE_BITBOARD_H
#include "types.h"

inline void setBit(uint64_t& bb, int sq)
{
    bb |= (ONE << sq);
}
inline void clearBit(uint64_t& bb, int sq)
{
    bb &= ~(ONE << sq);
}
inline bool checkBit(const uint64_t bb, int sq)
{
    return (bb >> sq) & ONE;
}
inline int bitScanForward(uint64_t bb)
{
    return __builtin_ctzll(bb);
}
inline int poplsb(uint64_t& bb) {
    int lsb = bitScanForward(bb);
    bb &= bb - 1;
    return lsb;
}
inline int fileIndex(int sq)
{
    return sq & 7;
}
inline int rankIndex(const int sq)
{
    return sq>>3;
}
inline int squareIndex(const int rank, const int file)
{
    return rank*8 + file;
}
inline int squareIndex(const std::string& s)
{
    return squareIndex(s[1] - '1', s[0] - 'a');
}
inline int mirrorVertically(const int sq)
{
    return sq ^ 56;
}
inline int mirrorHorizontally(const int sq)
{
    return sq ^ 7;
}
inline int pieceType(int piece)
{
    return piece & 0x07;
}
inline int pieceIndex(const int color, const int piece_type)
{
    return color*8 + piece_type;
}
inline int popcount64(const uint64_t x)
{
    return __builtin_popcountll(x);
}
inline int pieceColor(const int piece)
{
    return checkBit(piece, 3);
}
void printBitboard(uint64_t bb);
#endif //DEVRE_BITBOARD_H

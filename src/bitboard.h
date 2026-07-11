#ifndef DEVRE_BITBOARD_H
#define DEVRE_BITBOARD_H

#include "types.h"

constexpr void setBit(uint64_t& bb, int sq) { bb |= (ONE << sq); }

constexpr void clearBit(uint64_t& bb, int sq) { bb &= ~(ONE << sq); }

constexpr bool checkBit(const uint64_t bb, int sq) { return (bb >> sq) & ONE; }

constexpr int bitScanForward(uint64_t bb) { return __builtin_ctzll(bb); }

constexpr int poplsb(uint64_t& bb) {
    int lsb = bitScanForward(bb);
    bb &= bb - 1;
    return lsb;
}

constexpr int fileIndex(int sq) { return sq & 7; }

constexpr int rankIndex(const int sq) { return sq >> 3; }

constexpr int squareIndex(const int rank, const int file) { return rank * 8 + file; }

inline int squareIndex(const std::string& s) { return squareIndex(s[1] - '1', s[0] - 'a'); }

constexpr int mirrorVertically(const int sq) { return sq ^ 56; }

constexpr int mirrorHorizontally(const int sq) { return sq ^ 7; }

constexpr int pieceType(int piece) { return piece & 0x07; }

constexpr int pieceIndex(const int color, const int piece_type) { return color * 8 + piece_type; }

constexpr int popcount64(const uint64_t x) { return __builtin_popcountll(x); }

constexpr int pieceColor(const int piece) { return checkBit(piece, 3); }

void printBitboard(uint64_t bb);

#endif  //DEVRE_BITBOARD_H

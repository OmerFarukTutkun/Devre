#ifndef _BITBOARDS_H_
#define _BITBOARDS_H_

#include "stdint.h"
#include "stdbool.h"

#ifdef WIN32
    #include <Windows.h>
    #include <io.h>
#else
    #include <sys/time.h>
    #include <unistd.h>
#endif

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

enum Colors{
    WHITE,
    BLACK,
};
enum Pieces{
    PAWN   = 0, 
    KNIGHT = 1, 
    BISHOP = 2, 
    ROOK   = 3,  
    QUEEN  = 4, 
    KING   = 5,
    BLACK_PAWN   = 6, 
    BLACK_KNIGHT = 7,
    BLACK_BISHOP = 8, 
    BLACK_ROOK   = 9, 
    BLACK_QUEEN  = 10,  
    BLACK_KING   = 11,
    EMPTY = 255,
};

static const char piece_notations[] = "PNBRQKpnbrqk";

static const uint64_t ONE = 1ull;

static const char square_to_string[][3] = {
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", 
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", 
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", 
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", 
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};

enum Squares{
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NO_SQ = 255
};
enum {
    RANK_1 = 0x00000000000000FFULL,
    RANK_2 = 0x000000000000FF00ULL,
    RANK_3 = 0x0000000000FF0000ULL,
    RANK_4 = 0x00000000FF000000ULL,
    RANK_5 = 0x000000FF00000000ULL,
    RANK_6 = 0x0000FF0000000000ULL,
    RANK_7 = 0x00FF000000000000ULL,
    RANK_8 = 0xFF00000000000000ULL,

    FILE_A = 0x0101010101010101ULL,
    FILE_B = 0x0202020202020202ULL,
    FILE_C = 0x0404040404040404ULL,
    FILE_D = 0x0808080808080808ULL,
    FILE_E = 0x1010101010101010ULL,
    FILE_F = 0x2020202020202020ULL,
    FILE_G = 0x4040404040404040ULL,
    FILE_H = 0x8080808080808080ULL
};

void set_bit(uint64_t* bb, uint8_t sq);
void clear_bit(uint64_t* bb, uint8_t sq);
int check_bit(uint64_t bb, uint8_t sq);
int bitScanForward(uint64_t bb);
int poplsb(uint64_t *bb);
uint8_t file_index(uint8_t sq);
uint8_t rank_index(uint8_t sq);
uint8_t square_index(uint8_t rank, uint8_t file);
uint8_t piece_type(uint8_t piece);
uint8_t piece_index(uint8_t color, uint8_t piece_type);
uint8_t mirror_vertically(uint8_t sq);
uint8_t mirror_horizontally(uint8_t sq);
void print_bitboard(uint64_t bb);
int popcount64(uint64_t x);
#endif

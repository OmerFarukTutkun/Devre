#ifndef DEVRE_TYPES_H
#define DEVRE_TYPES_H

#include <chrono>
#include <cmath>
#include <ctime>
#include <iostream>
#include <string>
#include <iostream>
#include <map>
#include <unordered_map>
#include <cstring>
#include "vector"
#include "array"

#ifndef VERSION
#define VERSION "4.21"
#endif

constexpr auto MAX_PLY = 100;
constexpr auto START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

enum Score : int16_t {
    MAX_MATE_SCORE = 32000,
    VALUE_INFINITE = 32500,
    MIN_MATE_SCORE = MAX_MATE_SCORE - MAX_PLY
};

enum Color : uint8_t {
    WHITE,
    BLACK,
    N_COLORS = 2
};

constexpr Color operator~(Color C) { return Color(C ^ BLACK); }

enum PieceType : uint8_t {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    N_PIECE_TYPES = 6
};

enum Piece : uint8_t {
    WHITE_PAWN = 0,
    WHITE_KNIGHT = 1,
    WHITE_BISHOP = 2,
    WHITE_ROOK = 3,
    WHITE_QUEEN = 4,
    WHITE_KING = 5,
    BLACK_PAWN = 8,
    BLACK_KNIGHT = 9,
    BLACK_BISHOP = 10,
    BLACK_ROOK = 11,
    BLACK_QUEEN = 12,
    BLACK_KING = 13,
    N_PIECES = 14,
    EMPTY = 15,
};

enum Square : uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    N_SQUARES = 64, NO_SQ = 65,
};

enum Direction : int8_t {
    NORTH = 8,
    SOUTH = -8,
    WEST = -1,
    EAST = 1,
    NORTH_WEST = 7,
    NORTH_EAST = 9,
    SOUTH_WEST = -9,
    SOUTH_EAST = -7,
    N_DIRECTIONS = 8
};
enum Castle : uint8_t {
    WHITE_SHORT_CASTLE = 1,
    WHITE_LONG_CASTLE = 2,
    BLACK_SHORT_CASTLE = 4,
    BLACK_LONG_CASTLE = 8,
    WHITE_CASTLE = 3,
    BLACK_CASTLE = 12,
};
enum FileMask : uint64_t {
    FILE_H_BB = 0x8080808080808080ULL,
    FILE_G_BB = FILE_H_BB >> 1,
    FILE_F_BB = FILE_H_BB >> 2,
    FILE_E_BB = FILE_H_BB >> 3,
    FILE_D_BB = FILE_H_BB >> 4,
    FILE_C_BB = FILE_H_BB >> 5,
    FILE_B_BB = FILE_H_BB >> 6,
    FILE_A_BB = FILE_H_BB >> 7,
};
enum RankMask : uint64_t {
    RANK_1_BB = 0x00000000000000FFULL,
    RANK_2_BB = RANK_1_BB << 8,
    RANK_3_BB = RANK_1_BB << 16,
    RANK_4_BB = RANK_1_BB << 24,
    RANK_5_BB = RANK_1_BB << 32,
    RANK_6_BB = RANK_1_BB << 40,
    RANK_7_BB = RANK_1_BB << 48,
    RANK_8_BB = RANK_1_BB << 56,
};


constexpr uint64_t DEFAULT_CHECKMASK = 0xFFFFFFFFFFFFFFFFULL;

constexpr char const *SQUARE_IDENTIFIER[]{
        "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
        "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
        "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
        "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", " ", "-"
};

static std::unordered_map<char, uint8_t> CHAR_TO_PIECE({
                                                               {'P', WHITE_PAWN},
                                                               {'N', WHITE_KNIGHT},
                                                               {'B', WHITE_BISHOP},
                                                               {'R', WHITE_ROOK},
                                                               {'Q', WHITE_QUEEN},
                                                               {'K', WHITE_KING},
                                                               {'p', BLACK_PAWN},
                                                               {'n', BLACK_KNIGHT},
                                                               {'b', BLACK_BISHOP},
                                                               {'r', BLACK_ROOK},
                                                               {'q', BLACK_QUEEN},
                                                               {'k', BLACK_KING},
                                                               {' ', EMPTY},
                                                       });

static std::unordered_map<uint8_t, char> PIECE_TO_CHAR({
                                                               {WHITE_PAWN,   'P'},
                                                               {WHITE_KNIGHT, 'N'},
                                                               {WHITE_BISHOP, 'B'},
                                                               {WHITE_ROOK,   'R'},
                                                               {WHITE_QUEEN,  'Q'},
                                                               {WHITE_KING,   'K'},
                                                               {BLACK_PAWN,   'p'},
                                                               {BLACK_KNIGHT, 'n'},
                                                               {BLACK_BISHOP, 'b'},
                                                               {BLACK_ROOK,   'r'},
                                                               {BLACK_QUEEN,  'q'},
                                                               {BLACK_KING,   'k'},
                                                               {EMPTY,        ' '},
                                                       });

constexpr uint64_t ONE = 1ULL;

enum TT_BOUND {
    TT_NONE,
    TT_UPPERBOUND,
    TT_LOWERBOUND,
    TT_EXACT,
};

struct TTentry {
    uint64_t key{};
    int16_t score{};
    uint16_t move{};
    int16_t staticEval{};
    uint8_t depth{};
    uint8_t ageAndBound{};
};

struct nnueChange {
    uint8_t piece;
    uint8_t sq;
    int8_t sign;

    nnueChange(uint8_t piece, uint8_t sq, int8_t sign) :
            piece(piece),
            sq(sq),
            sign(sign) {}
};

using PieceTo = int16_t[N_PIECES][N_SQUARES];

//TODO: maybe try reading these values from .nnue file instead of hardcoding
auto constexpr INPUT_SIZE = 32 * 768;
auto constexpr L1 = 512;

auto constexpr SCALE_OUTPUT = 256; // scaling needed at the output because of sigmoid
auto constexpr SCALE_WEIGHT = 512; // 512 may result in overflow in NNUE, but I guess it doesn't happen in practice. So for better accuracy, I chose 512
auto constexpr OUTPUT_DIVISOR = ((SCALE_WEIGHT * SCALE_WEIGHT) / SCALE_OUTPUT);


class NNUEData {
public:
    alignas(64) int16_t accumulator[MAX_PLY + 10][2][L1]{};
    int size{};
    uint16_t move{};
    std::vector<nnueChange> nnueChanges;

    NNUEData() {
        nnueChanges.reserve(N_SQUARES);
        nnueChanges.clear();
        size = 0;
    }

    void addAccumulator() {
        size++;
    }

    void clear() {
        size = 0;
    }

    void popAccumulator() {
        size = std::max(0, size - 1);
    }
};

struct BoardHistory {
    //store the information we cannot get back
    uint8_t enPassant;
    uint8_t castlings;
    uint8_t capturedPiece;
    uint8_t halfMove;
    uint64_t key;

    BoardHistory(uint8_t enPassant, uint8_t castling, uint8_t halfMove, uint8_t capturedPiece, uint64_t key)
            : enPassant(enPassant),
              castlings(castling),
              halfMove(halfMove),
              capturedPiece(capturedPiece),
              key(key) {}
};

#endif //DEVRE_TYPES_H

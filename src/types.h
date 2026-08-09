#ifndef DEVRE_TYPES_H
#define DEVRE_TYPES_H

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#ifndef VERSION
    #define VERSION "7.01"
#endif

constexpr auto MAX_PLY   = 100;
constexpr auto START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

// Internal eval units per reported pawn. Fitted on 3.2M positions from 37836
// self-play games: a position worth this much internally is won half the time,
// so "score cp 100" means a 50% win probability, as in Stockfish. Only the
// reported score is scaled; search and datagen keep the internal units.
constexpr int NORMALIZE_TO_PAWN = 325;

enum Score : int16_t {
    MAX_MATE_SCORE = 32000,
    VALUE_INFINITE = 32500,
    MIN_MATE_SCORE = MAX_MATE_SCORE - MAX_PLY,
    TB_SCORE       = MIN_MATE_SCORE - 1,
    MIN_TB_SCORE   = TB_SCORE - MAX_PLY,
    SCORE_NONE     = MAX_MATE_SCORE + 1
};

enum Color : uint8_t {
    WHITE = 0,
    BLACK = 1,
    N_COLORS = 2
};

constexpr Color operator~(Color C) { return Color(C ^ BLACK); }

enum PieceType : uint8_t {
    PAWN          = 0,
    KNIGHT        = 1,
    BISHOP        = 2,
    ROOK          = 3,
    QUEEN         = 4,
    KING          = 5,
    N_PIECE_TYPES = 6
};

enum Piece : uint8_t {
    WHITE_PAWN   = 0,
    WHITE_KNIGHT = 1,
    WHITE_BISHOP = 2,
    WHITE_ROOK   = 3,
    WHITE_QUEEN  = 4,
    WHITE_KING   = 5,
    BLACK_PAWN   = 8,
    BLACK_KNIGHT = 9,
    BLACK_BISHOP = 10,
    BLACK_ROOK   = 11,
    BLACK_QUEEN  = 12,
    BLACK_KING   = 13,
    N_PIECES     = 14,
    EMPTY        = 15,
};

enum Square : uint8_t {
    A1 = 0,
    B1 = 1,
    C1 = 2,
    D1 = 3,
    E1 = 4,
    F1 = 5,
    G1 = 6,
    H1 = 7,
    A2 = 8,
    B2 = 9,
    C2 = 10,
    D2 = 11,
    E2 = 12,
    F2 = 13,
    G2 = 14,
    H2 = 15,
    A3 = 16,
    B3 = 17,
    C3 = 18,
    D3 = 19,
    E3 = 20,
    F3 = 21,
    G3 = 22,
    H3 = 23,
    A4 = 24,
    B4 = 25,
    C4 = 26,
    D4 = 27,
    E4 = 28,
    F4 = 29,
    G4 = 30,
    H4 = 31,
    A5 = 32,
    B5 = 33,
    C5 = 34,
    D5 = 35,
    E5 = 36,
    F5 = 37,
    G5 = 38,
    H5 = 39,
    A6 = 40,
    B6 = 41,
    C6 = 42,
    D6 = 43,
    E6 = 44,
    F6 = 45,
    G6 = 46,
    H6 = 47,
    A7 = 48,
    B7 = 49,
    C7 = 50,
    D7 = 51,
    E7 = 52,
    F7 = 53,
    G7 = 54,
    H7 = 55,
    A8 = 56,
    B8 = 57,
    C8 = 58,
    D8 = 59,
    E8 = 60,
    F8 = 61,
    G8 = 62,
    H8 = 63,
    N_SQUARES = 64,
    NO_SQ     = 65,
};

enum Direction : int8_t {
    NORTH        = 8,
    SOUTH        = -8,
    WEST         = -1,
    EAST         = 1,
    NORTH_WEST   = 7,
    NORTH_EAST   = 9,
    SOUTH_WEST   = -9,
    SOUTH_EAST   = -7,
    N_DIRECTIONS = 8
};
enum Castle : uint8_t {
    WHITE_SHORT_CASTLE = 1,
    WHITE_LONG_CASTLE  = 2,
    BLACK_SHORT_CASTLE = 4,
    BLACK_LONG_CASTLE  = 8,
    WHITE_CASTLE       = 3,
    BLACK_CASTLE       = 12,
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

constexpr char const* SQUARE_IDENTIFIER[]{"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2", "a3", "b3", "c3", "d3", "e3", "f3",
                                          "g3", "h3", "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4", "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5", "a6", "b6", "c6", "d6",
                                          "e6", "f6", "g6", "h6", "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7", "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8", " ",  "-"};

// Piece values index this table directly (unused slots and EMPTY are spaces).
constexpr char pieceToChar(int piece) {
    constexpr char table[N_PIECES + 3] = {'P', 'N', 'B', 'R', 'Q', 'K', ' ', ' ', 'p', 'n', 'b', 'r', 'q', 'k', ' ', ' '};
    return table[piece];
}

constexpr uint8_t charToPiece(char c) {
    switch (c)
    {
    case 'P' :
        return WHITE_PAWN;
    case 'N' :
        return WHITE_KNIGHT;
    case 'B' :
        return WHITE_BISHOP;
    case 'R' :
        return WHITE_ROOK;
    case 'Q' :
        return WHITE_QUEEN;
    case 'K' :
        return WHITE_KING;
    case 'p' :
        return BLACK_PAWN;
    case 'n' :
        return BLACK_KNIGHT;
    case 'b' :
        return BLACK_BISHOP;
    case 'r' :
        return BLACK_ROOK;
    case 'q' :
        return BLACK_QUEEN;
    case 'k' :
        return BLACK_KING;
    default :
        return EMPTY;
    }
}

constexpr uint64_t ONE = 1ULL;

enum TT_BOUND : uint8_t {
    TT_NONE,
    TT_UPPERBOUND,
    TT_LOWERBOUND,
    TT_EXACT,
};

struct __attribute__((__packed__)) TTentry {
    uint32_t key{0};
    int16_t  score{0};
    uint16_t move{0};
    int16_t  staticEval{0};
    uint8_t  depth{0};
    uint8_t  ageBound{0};
};
constexpr int numEntryPerBucket = 5;

struct __attribute__((__packed__)) TTBucket {
    TTentry  entries[numEntryPerBucket];
    uint32_t padding{};
};


struct nnueChange {
    int8_t piece;
    int8_t sq;
    int8_t sign;

    nnueChange(int piece = 0, int sq = 0, int sign = 0) :
        piece(static_cast<int8_t>(piece)),
        sq(static_cast<int8_t>(sq)),
        sign(static_cast<int8_t>(sign)) {}
};

using PieceTo = int16_t[N_PIECES][N_SQUARES];

// One sparse index space: 12*768 king-bucketed piece-square features followed
// by 96*95/2 pawn pairs. NNUE_FT_OUT must match the trainer's FT constant in
constexpr int NNUE_FT_IN = 12 * 768 + 96 * 95 / 2;  // 13776
constexpr int NNUE_FT_OUT = 1024;

struct NNUEAccumulator {
    alignas(64) int16_t data[2][NNUE_FT_OUT]{};

    // Board state AFTER this ply, needed by the incremental update and filled
    // by `Board::makeMove`. The pawn bitboards drive the pawn-pair delta, and
    // the king squares decide whether a perspective can be updated at all: a
    // king that crosses a bucket or mirror boundary re-indexes every psq
    // feature and forces that perspective to be rebuilt.
    uint64_t pawns[N_COLORS]{};
    uint8_t  kingSq[N_COLORS]{};
    bool     stateValid{};

    nnueChange changes[4]{};
    uint8_t changeCount{};
    // Per perspective, because one side's king can force a rebuild while the
    // other side's accumulator stays incrementally reachable.
    bool computed[N_COLORS]{};

    void clear() {
        computed[WHITE] = computed[BLACK] = false;
        stateValid      = false;
        changeCount     = 0;
    }

    void invalidate() { clear(); }

    void clearChanges() { changeCount = 0; }

    void addChange(int piece, int sq, int sign) {
        if (changeCount < 4)
            changes[changeCount++] = {piece, sq, sign};
    }
};

static_assert(alignof(NNUEAccumulator) == 64, "simd.h issues aligned loads on NNUEAccumulator::data");

/// One cached accumulator per (perspective, king bucket, mirror), together with
/// the piece placement it was built from. A king that changes bucket cannot be
/// updated with deltas, but it can be rebuilt as a delta from the last position
/// that used the SAME bucket instead of from the bias, which is normally a
/// handful of rows rather than all ~68.
struct NNUERefreshEntry {
    alignas(64) int16_t data[NNUE_FT_OUT]{};
    uint64_t pieces[N_PIECES]{};
    bool     initialised{};
};

class NNUEData {
   public:
    static constexpr int REFRESH_BUCKETS = 12;
    static constexpr int REFRESH_ENTRIES = N_COLORS * REFRESH_BUCKETS * 2;  // x2 for the mirror

    std::vector<NNUEAccumulator>  accumulator;
    std::vector<NNUERefreshEntry> refreshCache;
    int                           size{};

    NNUEData() : accumulator(MAX_PLY + 10), refreshCache(REFRESH_ENTRIES) {}

    void clearRefreshCache() {
        for (auto& entry : refreshCache)
            entry.initialised = false;
    }
};

struct BoardHistory {
    //store the information we cannot get back
    uint8_t  enPassant;
    uint8_t  castlings;
    uint8_t  capturedPiece;
    uint8_t  halfMove;
    uint64_t key;

    BoardHistory(uint8_t enPassant, uint8_t castling, uint8_t capturedPiece, uint8_t halfMove, uint64_t key) :
        enPassant(enPassant),
        castlings(castling),
        capturedPiece(capturedPiece),
        halfMove(halfMove),
        key(key) {}
};

#endif  //DEVRE_TYPES_H


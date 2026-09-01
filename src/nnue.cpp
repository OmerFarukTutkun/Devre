#include "nnue.h"

#include "attack.h"
#include "simd.h"
#include "util.h"
#include "incbin/incbin.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#ifndef NET
    #define NET "quantised.bin"
#endif

INCBIN(EmbeddedNet, NET);

namespace {

// clang-format off
// MUST match devre_threat.rs's expand_mirrored(KING_BUCKET_HALF).
constexpr uint8_t KING_BUCKET_LAYOUT[64] = {
     0,  1,  2,  3,  3,  2,  1,  0,
     4,  5,  6,  7,  7,  6,  5,  4,
     8,  8,  9,  9,  9,  9,  8,  8,
    10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
};
// clang-format on

/// Only same/adjacent-file pawns pair, so partners are one AND away.
constexpr uint64_t adjacentFiles(int file) {
    uint64_t mask = FILE_A_BB << file;
    if (file > 0)
        mask |= FILE_A_BB << (file - 1);
    if (file < 7)
        mask |= FILE_A_BB << (file + 1);
    return mask;
}

constexpr std::array<uint64_t, 8> ADJACENT_FILES = [] {
    std::array<uint64_t, 8> table{};
    for (int file = 0; file < 8; file++)
        table[file] = adjacentFiles(file);
    return table;
}();

// --- Tactical Threat Tables (constexpr) -------------------------------------

struct ThreatTables {
    int8_t   pawn_map[12];
    struct NonPkData {
        uint64_t attacks[64];
        uint16_t indices[64];
        int8_t   targets[12];
        uint16_t count;
    } non_pk[4]; // 0=Knight, 1=Bishop, 2=Rook, 3=Queen
    uint32_t offsets[5];
    int16_t  non_pk_index[4][64][64];
    int16_t  pawn_index[64][64];

    constexpr ThreatTables() : pawn_map{}, non_pk{}, offsets{}, non_pk_index{}, pawn_index{} {
        for (int i = 0; i < 12; i++) {
            pawn_map[i] = -1;
            for (int p = 0; p < 4; p++) non_pk[p].targets[i] = -1;
        }

        // Relative targets: 0..5 Friendly, 6..11 Enemy
        // Pawn targets: Knight (1), Rook (3) -> Friendly 0, 1; Enemy 2, 3
        pawn_map[1] = 0;
        pawn_map[3] = 1;
        pawn_map[7] = 2;
        pawn_map[9] = 3;

        // Knight targets: PAWN(0), KNIGHT(1), BISHOP(2), ROOK(3), QUEEN(4) (N=5)
        non_pk[0].targets[0] = 0; non_pk[0].targets[6]  = 5;
        non_pk[0].targets[1] = 1; non_pk[0].targets[7]  = 6;
        non_pk[0].targets[2] = 2; non_pk[0].targets[8]  = 7;
        non_pk[0].targets[3] = 3; non_pk[0].targets[9]  = 8;
        non_pk[0].targets[4] = 4; non_pk[0].targets[10] = 9;

        // Bishop targets: PAWN(0), KNIGHT(1), BISHOP(2), ROOK(3) (N=4)
        non_pk[1].targets[0] = 0; non_pk[1].targets[6]  = 4;
        non_pk[1].targets[1] = 1; non_pk[1].targets[7]  = 5;
        non_pk[1].targets[2] = 2; non_pk[1].targets[8]  = 6;
        non_pk[1].targets[3] = 3; non_pk[1].targets[9]  = 7;

        // Rook targets: PAWN(0), KNIGHT(1), BISHOP(2), ROOK(3) (N=4)
        non_pk[2].targets[0] = 0; non_pk[2].targets[6]  = 4;
        non_pk[2].targets[1] = 1; non_pk[2].targets[7]  = 5;
        non_pk[2].targets[2] = 2; non_pk[2].targets[8]  = 6;
        non_pk[2].targets[3] = 3; non_pk[2].targets[9]  = 7;

        // Queen targets: PAWN(0), KNIGHT(1), BISHOP(2), ROOK(3), QUEEN(4) (N=5)
        non_pk[3].targets[0] = 0; non_pk[3].targets[6]  = 5;
        non_pk[3].targets[1] = 1; non_pk[3].targets[7]  = 6;
        non_pk[3].targets[2] = 2; non_pk[3].targets[8]  = 7;
        non_pk[3].targets[3] = 3; non_pk[3].targets[9]  = 8;
        non_pk[3].targets[4] = 4; non_pk[3].targets[10] = 9;

        for (int sq = 0; sq < 64; sq++) {
            non_pk[0].attacks[sq] = KnightAttacks[sq];
            non_pk[0].indices[sq] = non_pk[0].count;
            non_pk[0].count += popcount64(non_pk[0].attacks[sq]);

            non_pk[1].attacks[sq] = DIR_RAYS.diag[sq];
            non_pk[1].indices[sq] = non_pk[1].count;
            non_pk[1].count += popcount64(non_pk[1].attacks[sq]);

            non_pk[2].attacks[sq] = DIR_RAYS.straight[sq];
            non_pk[2].indices[sq] = non_pk[2].count;
            non_pk[2].count += popcount64(non_pk[2].attacks[sq]);

            non_pk[3].attacks[sq] = DIR_RAYS.diag[sq] | DIR_RAYS.straight[sq];
            non_pk[3].indices[sq] = non_pk[3].count;
            non_pk[3].count += popcount64(non_pk[3].attacks[sq]);
        }

        offsets[0] = 4 * 84;
        offsets[1] = offsets[0] + 10 * non_pk[0].count;
        offsets[2] = offsets[1] + 8 * non_pk[1].count;
        offsets[3] = offsets[2] + 8 * non_pk[2].count;
        offsets[4] = offsets[3] + 10 * non_pk[3].count;

        for (int p = 0; p < 4; p++) {
            for (int src = 0; src < 64; src++) {
                for (int dest = 0; dest < 64; dest++) {
                    uint64_t mask = (1ULL << dest) - 1ULL;
                    int count = popcount64(non_pk[p].attacks[src] & mask);
                    non_pk_index[p][src][dest] = non_pk[p].indices[src] + count;
                }
            }
        }

        for (int src = 0; src < 64; src++) {
            for (int dest = 0; dest < 64; dest++) {
                int diff = (dest > src) ? (dest - src) : (src - dest);
                int expected = (dest > src) ? 7 : 9;
                int id = (diff == expected) ? 0 : 1;
                int attack = 2 * (src % 8) + id - 1;
                int rank = src / 8;
                if (rank < 1 || rank > 6 || attack < 0 || attack >= 14) {
                    pawn_index[src][dest] = -1;
                } else {
                    pawn_index[src][dest] = (rank - 1) * 14 + attack;
                }
            }
        }
    }
};
constexpr ThreatTables THREAT_TABLES{};

// A wrong attack mask shifts every offset after it and silently repoints the
// threat features, so pin the totals.
static_assert(THREAT_TABLES.non_pk[0].count == 336, "knight attack table is wrong");
static_assert(THREAT_TABLES.non_pk[1].count == 560, "bishop attack table is wrong");
static_assert(THREAT_TABLES.non_pk[2].count == 896, "rook attack table is wrong");
static_assert(THREAT_TABLES.non_pk[3].count == 1456, "queen attack table is wrong");
static_assert(2 * THREAT_TABLES.offsets[4] == NNUE::NUM_THREATS,
              "threat index space disagrees with the trainer");

constexpr int mapThreatSingle(const ThreatTables& tbl, int piece, int src, int dest, int target) {
    if (target < 0 || target >= 12) return -1;
    if (piece == PAWN) {
        int targetId = tbl.pawn_map[target];
        if (targetId < 0) return -1;
        int baseIdx = tbl.pawn_index[src][dest];
        if (baseIdx < 0) return -1;
        return targetId * 84 + baseIdx;
    } else {
        int pieceIdx = piece - 1; // Knight=1 -> 0, Bishop=2 -> 1, Rook=3 -> 2, Queen=4 -> 3
        if (pieceIdx < 0 || pieceIdx >= 4) return -1;
        const auto& pk = tbl.non_pk[pieceIdx];
        int piece_target = pk.targets[target];
        if (piece_target < 0) return -1;
        if (dest > src && ((target % 6) == piece)) return -1;
        int index = tbl.non_pk_index[pieceIdx][src][dest];
        return tbl.offsets[pieceIdx] + piece_target * pk.count + index;
    }
}

struct FlatThreatTable {
    int16_t map[5][64][64][12];
    constexpr FlatThreatTable() : map{} {
        for (int p = 0; p < 5; p++) {
            for (int s = 0; s < 64; s++) {
                for (int d = 0; d < 64; d++) {
                    for (int t = 0; t < 12; t++) {
                        map[p][s][d][t] = static_cast<int16_t>(mapThreatSingle(THREAT_TABLES, p, s, d, t));
                    }
                }
            }
        }
    }
};
constexpr FlatThreatTable FLAT_THREAT_TABLE{};

// 480 KB, against 40 KB for computing the index on the fly; measured 5.7% faster.
inline int mapThreatFast(int piece, int src, int dest, int target) {
    if (static_cast<unsigned>(piece) >= 5 || static_cast<unsigned>(target) >= 12) return -1;
    return FLAT_THREAT_TABLE.map[piece][src][dest][target];
}

// --- Accumulator arithmetic -------------------------------------------------

// The psq half has no bias of its own; the FT bias lives in the tac half.
alignas(64) constexpr int16_t ZERO_ACC[NNUE_FT_OUT] = {};

// One line is enough; the loop walks the row front to back. Worth ~5% of
// accumulator time, more lines cost more in load slots than they save.
inline void prefetchRow(const int8_t* row) { __builtin_prefetch(row, 0, 3); }

// Measured flat from 4 to 8, worse either side.
constexpr uint32_t ACC_BLOCK = 4;
static_assert(NNUE_FT_OUT % (ACC_BLOCK * SIMD::vecSize) == 0, "column blocks must tile the accumulator");

/// out = base + sum(addRows) - sum(subRows). Rows are int8, widened on load.
inline __attribute__((always_inline)) void
accumulateRows(int16_t* out, const int16_t* base, const int8_t* const* addRows, int nAdd,
               const int8_t* const* subRows, int nSub) {
    if (nAdd == 0 && nSub == 0) {
        std::memcpy(out, base, sizeof(int16_t) * NNUE_FT_OUT);
        return;
    }

    constexpr uint32_t step = ACC_BLOCK * SIMD::vecSize;
    for (uint32_t i = 0; i < NNUE_FT_OUT; i += step) {
        SIMD::vecType acc[ACC_BLOCK];
        for (uint32_t v = 0; v < ACC_BLOCK; v++)
            acc[v] = SIMD::vecLoad(base + i + v * SIMD::vecSize);

        for (int a = 0; a < nAdd; a++) {
            const int8_t* row = addRows[a];
            for (uint32_t v = 0; v < ACC_BLOCK; v++)
                acc[v] = SIMD::vecAddEpi16(acc[v], SIMD::vecLoadI8ToI16(row + i + v * SIMD::vecSize));
        }

        for (int s = 0; s < nSub; s++) {
            const int8_t* row = subRows[s];
            for (uint32_t v = 0; v < ACC_BLOCK; v++)
                acc[v] = SIMD::vecSubEpi16(acc[v], SIMD::vecLoadI8ToI16(row + i + v * SIMD::vecSize));
        }

        for (uint32_t v = 0; v < ACC_BLOCK; v++)
            SIMD::vecStore(out + i + v * SIMD::vecSize, acc[v]);
    }
}

inline float crelu(float x) { return std::clamp(x, 0.0f, 1.0f); }

inline float screlu(float x) {
    const float c = crelu(x);
    return c * c;
}

// --- File format ------------------------------------------------------------

constexpr char     MAGIC[8]    = {'D', 'V', 'N', 'N', 'U', 'E', '5', '\0'};
constexpr uint32_t NET_VERSION = 1;
constexpr size_t   HEADER_LEN  = 48;

uint32_t readU32(const uint8_t* p, size_t at) {
    uint32_t v;
    std::memcpy(&v, p + at, sizeof(v));
    return v;
}

}  // namespace

NNUE NNUE::instance;

NNUE::NNUE() {
    if (!loadFromBuffer(gEmbeddedNetData, gEmbeddedNetSize, "<embedded>"))
        std::cout << "info string No usable embedded net; set EvalFile" << std::endl;
}

// --- Feature indexing -------------------------------------------------------

NNUE::PerspectiveKey NNUE::perspectiveKey(int kingSquare, Color perspective) {
    const int mirrored = (kingSquare & 7) > 3 ? 7 : 0;
    const int oriented = perspective == BLACK ? mirrorVertically(kingSquare) : kingSquare;
    return {KING_BUCKET_LAYOUT[oriented], static_cast<uint8_t>((perspective == BLACK ? 56 : 0) ^ mirrored)};
}

int NNUE::psqFeature(int piece, int square, Color perspective, PerspectiveKey key) {
    const int relativeSquare = square ^ key.flip;
    const int relativeColor  = pieceColor(piece) != perspective;
    return 768 * key.bucket + 384 * relativeColor + 64 * pieceType(piece) + relativeSquare;
}

int NNUE::pawnId(int square, Color pawnColor, Color perspective, int flip) {
    const int base = (square ^ flip) - 8;
    return base + (pawnColor != perspective ? 48 : 0);
}

int NNUE::pawnPairIndex(int idA, int idB) {
    const int lo = std::min(idA, idB);
    const int hi = std::max(idA, idB);
    return hi * (hi - 1) / 2 + lo;
}

// --- Full refresh -----------------------------------------------------------

/// Rebuilds one or both halves; a nullptr skips that half, and skipping tac
/// skips the threat generation, which is what a refresh actually costs.
/// Rebuilds one or both halves; a nullptr skips that half, and skipping tac
/// skips the threat generation, which is what a refresh actually costs.
void NNUE::refresh(const Board& board, Color perspective, int16_t* psqOut, int16_t* tacOut) const {
    const PerspectiveKey key = perspectiveKey(bitScanForward(board.bitboards[pieceIndex(perspective, KING)]),
                                              perspective);
    if (psqOut)
        refreshPsq(board, perspective, key, psqOut);
    if (tacOut)
        refreshTac(board, perspective, key, tacOut);
}

/// One row per piece on the board.
void NNUE::refreshPsq(const Board& board, Color perspective, PerspectiveKey key, int16_t* out) const {
    const NetworkData& net = *network;

    const int8_t* rows[MAX_PSQ_ACTIVE];
    int           n = 0;

    uint64_t occupied = board.occupied[WHITE] | board.occupied[BLACK];
    while (occupied && n < MAX_PSQ_ACTIVE)
    {
        const int sq = poplsb(occupied);
        rows[n++]    = net.ftPsqWeights
                  + static_cast<size_t>(psqFeature(board.pieceBoard[sq], sq, perspective, key)) * NNUE_FT_OUT;
    }

    accumulateRows(out, ZERO_ACC, rows, n, nullptr, 0);
}

/// Pawn-structure pairs plus every threat on the board -- the expensive half,
/// which is why a bucket-only king move rebuilds psq without touching this.
void NNUE::refreshTac(const Board& board, Color perspective, PerspectiveKey key, int16_t* out) const {
    const NetworkData& net = *network;

    const int8_t* tacRows[MAX_PAIR_ACTIVE + MAX_THREAT_ACTIVE];
    int           nTac = 0;

    int pawnIds[16];
    int pawnFiles[16];
    int pawns = 0;

    uint64_t pawnBB = board.bitboards[WHITE_PAWN] | board.bitboards[BLACK_PAWN];
    while (pawnBB)
    {
        const int sq     = poplsb(pawnBB);
        pawnIds[pawns]   = pawnId(sq, static_cast<Color>(pieceColor(board.pieceBoard[sq])), perspective, key.flip);
        pawnFiles[pawns] = sq & 7;
        pawns++;
    }

    const uint64_t occ = board.occupied[WHITE] | board.occupied[BLACK];

    // Pawn pairs
    for (int i = 0; i < pawns; i++)
        for (int j = i + 1; j < pawns; j++)
            if (std::abs(pawnFiles[i] - pawnFiles[j]) <= 1)
                if (nTac < MAX_PAIR_ACTIVE + MAX_THREAT_ACTIVE)
                {
                    tacRows[nTac++] = net.ftTacWeights
                                    + static_cast<size_t>(pawnPairIndex(pawnIds[i], pawnIds[j]))
                                          * NNUE_FT_OUT;
                }

    // Every (attacker, victim) pair on the board. Pairs the feature space does
    // not cover come back as -1 and are dropped.
    const Color us        = perspective;
    const int   ourKingSq = bitScanForward(board.bitboards[pieceIndex(us, KING)]);
    const int   hFlip     = (ourKingSq % 8 > 3) ? 7 : 0;
    const int   flip      = (us == WHITE) ? hFlip : (56 ^ hFlip);

    /// Records the threat from `sq` onto whatever occupies `dest`.
    auto addThreat = [&](int piece, int sq, int dest, int offset) {
        const int victim = board.pieceBoard[dest];
        if (victim >= N_PIECES)  // EMPTY, i.e. pieceBoard disagreeing with occupied
            return;
        const int target = pieceType(victim) + (pieceColor(victim) == us ? 0 : 6);
        const int index  = mapThreatFast(piece, sq ^ flip, dest ^ flip, target);
        if (index >= 0 && nTac < MAX_PAIR_ACTIVE + MAX_THREAT_ACTIVE)
        {
            tacRows[nTac++] = net.ftTacWeights
                            + static_cast<size_t>(PAWN_PAIRS + offset + index) * NNUE_FT_OUT;
        }
    };

    /// Every occupied square `piece` on `sq` attacks.
    auto addThreatsFrom = [&](int piece, int sq, uint64_t attacks, int offset) {
        uint64_t targets = attacks & occ;
        while (targets)
            addThreat(piece, sq, poplsb(targets), offset);
    };

    for (const Color c : {us, ~us})
    {
        // The two colours occupy disjoint halves of the threat index space.
        const int offset = (c == us) ? 0 : THREAT_TABLES.offsets[4];

        // Pawns move as a block: shift the whole bitboard instead of walking it,
        // then read each attacker's origin back off the destination square.
        const uint64_t pawns = board.bitboards[pieceIndex(c, PAWN)];
        const int      up    = (c == WHITE) ? 8 : -8;
        uint64_t       east  = pawns & ~FILE_H_BB;
        uint64_t       west  = pawns & ~FILE_A_BB;
        east = ((c == WHITE) ? east << 9 : east >> 7) & occ;
        west = ((c == WHITE) ? west << 7 : west >> 9) & occ;
        while (east)
        {
            const int dest = poplsb(east);
            addThreat(PAWN, dest - up - 1, dest, offset);
        }
        while (west)
        {
            const int dest = poplsb(west);
            addThreat(PAWN, dest - up + 1, dest, offset);
        }

        uint64_t knights = board.bitboards[pieceIndex(c, KNIGHT)];
        while (knights)
        {
            const int sq = poplsb(knights);
            addThreatsFrom(KNIGHT, sq, KnightAttacks[sq], offset);
        }

        uint64_t bishops = board.bitboards[pieceIndex(c, BISHOP)];
        while (bishops)
        {
            const int sq = poplsb(bishops);
            addThreatsFrom(BISHOP, sq, bishopAttacks(occ, sq), offset);
        }

        uint64_t rooks = board.bitboards[pieceIndex(c, ROOK)];
        while (rooks)
        {
            const int sq = poplsb(rooks);
            addThreatsFrom(ROOK, sq, rookAttacks(occ, sq), offset);
        }

        uint64_t queens = board.bitboards[pieceIndex(c, QUEEN)];
        while (queens)
        {
            const int sq = poplsb(queens);
            addThreatsFrom(QUEEN, sq, bishopAttacks(occ, sq) | rookAttacks(occ, sq), offset);
        }
    }

    accumulateRows(out, net.ftTacBiases, tacRows, nTac, nullptr, 0);
}

// --- Incremental update -----------------------------------------------------

/// doPsq / doTac are independent: the psq chain breaks on any king-key change,
/// the tac chain only when the mirror flips.
void NNUE::applyPly(Board& board, Color perspective, int idx, PerspectiveKey key, bool doPsq, bool doTac) const {
    const NetworkData& net  = *network;
    auto&              cur  = board.nnueData.accumulator[idx];
    const auto&        prev = board.nnueData.accumulator[idx - 1];

    const int8_t* addRows[MAX_PAIR_ACTIVE + MAX_THREAT_ACTIVE];
    const int8_t* subRows[MAX_PAIR_ACTIVE + MAX_THREAT_ACTIVE];
    int           nAdd = 0;
    int           nSub = 0;

    auto rowPsq = [&](int feature) {
        const int8_t* row = net.ftPsqWeights + static_cast<size_t>(feature) * NNUE_FT_OUT;
        prefetchRow(row);
        return row;
    };

    if (doPsq)
    {
        for (int i = 0; i < cur.changeCount; i++)
        {
            const auto& change  = cur.changes[i];
            const int   feature = psqFeature(change.piece, change.sq, perspective, key);
            if (change.sign > 0)
                addRows[nAdd++] = rowPsq(feature);
            else
                subRows[nSub++] = rowPsq(feature);
        }

        accumulateRows(cur.psq[perspective], prev.psq[perspective], addRows, nAdd, subRows, nSub);
        cur.computedPsq[perspective] = true;
    }

    if (!doTac)
        return;

    nAdd = 0;
    nSub = 0;

    pawnPairDelta(prev.pawns, cur.pawns, perspective, key, addRows, nAdd, subRows, nSub);

    for (int i = 0; i < cur.threatChangeCount; i++)
    {
        const DirtyThreat& t      = cur.threatChanges[i];
        const int          offset = (t.pieceColor() == perspective) ? 0 : THREAT_TABLES.offsets[4];
        const int          target = t.targetPiece() + (t.targetColor() == perspective ? 0 : 6);
        const int          index  = mapThreatFast(t.piece(), t.sq ^ key.flip, t.dest ^ key.flip, target);
        if (index < 0)
            continue;

        const int8_t* row = net.ftTacWeights
                          + static_cast<size_t>(PAWN_PAIRS + offset + index) * NNUE_FT_OUT;
        prefetchRow(row);
        if (t.sign() > 0)
            addRows[nAdd++] = row;
        else
            subRows[nSub++] = row;
    }

    accumulateRows(cur.tac[perspective], prev.tac[perspective], addRows, nAdd, subRows, nSub);
    cur.computedTac[perspective] = true;
}

void NNUE::pawnPairDelta(const uint64_t prevPawns[N_COLORS], const uint64_t curPawns[N_COLORS], Color perspective,
                         PerspectiveKey key, const int8_t** addRows, int& nAdd, const int8_t** subRows,
                         int& nSub) const {
    if (prevPawns[WHITE] == curPawns[WHITE] && prevPawns[BLACK] == curPawns[BLACK])
        return;

    const NetworkData& net = *network;
    auto rowTac = [&](int feature) {
        const int8_t* row = net.ftTacWeights + static_cast<size_t>(feature) * NNUE_FT_OUT;
        prefetchRow(row);
        return row;
    };

    int removed[16], added[16];
    int nRemoved = 0, nAdded = 0;

    for (int c = 0; c < N_COLORS; c++)
    {
        const auto colour = static_cast<Color>(c);

        uint64_t gone = prevPawns[c] & ~curPawns[c];
        while (gone)
        {
            const int sq        = poplsb(gone);
            removed[nRemoved++] = pawnId(sq, colour, perspective, key.flip) | ((sq & 7) << 8);
        }

        uint64_t fresh = curPawns[c] & ~prevPawns[c];
        while (fresh)
        {
            const int sq    = poplsb(fresh);
            added[nAdded++] = pawnId(sq, colour, perspective, key.flip) | ((sq & 7) << 8);
        }
    }

    const uint64_t keptWhite = prevPawns[WHITE] & curPawns[WHITE];
    const uint64_t keptBlack = prevPawns[BLACK] & curPawns[BLACK];

    auto pairsWithKept = [&](const int* moved, int count, const int8_t** rows, int& n) {
        for (int i = 0; i < count; i++)
        {
            const int id   = moved[i] & 0xFF;
            const int file = moved[i] >> 8;

            uint64_t partners = keptWhite & ADJACENT_FILES[file];
            while (partners)
            {
                const int sq = poplsb(partners);
                rows[n++]    = rowTac(pawnPairIndex(id, pawnId(sq, WHITE, perspective, key.flip)));
            }

            partners = keptBlack & ADJACENT_FILES[file];
            while (partners)
            {
                const int sq = poplsb(partners);
                rows[n++]    = rowTac(pawnPairIndex(id, pawnId(sq, BLACK, perspective, key.flip)));
            }

            for (int j = i + 1; j < count; j++)
                if (std::abs(file - (moved[j] >> 8)) <= 1)
                    rows[n++] = rowTac(pawnPairIndex(id, moved[j] & 0xFF));
        }
    };

    pairsWithKept(removed, nRemoved, subRows, nSub);
    pairsWithKept(added, nAdded, addRows, nAdd);
}

void NNUE::refreshOnBucketChange(Board& board) const {
    if (!loaded)
        return;

    auto&     stack = board.nnueData.accumulator;
    const int cur   = board.nnueData.size;
    if (cur == 0 || !stack[cur - 1].stateValid)
        return;

    for (const Color p : {WHITE, BLACK})
    {
        const PerspectiveKey kNow  = perspectiveKey(stack[cur].kingSq[p], p);
        const PerspectiveKey kPrev = perspectiveKey(stack[cur - 1].kingSq[p], p);
        if (kNow == kPrev)
            continue;
        if (kNow.flip != kPrev.flip)
        {
            // Mirror crossed: every feature index changes, both halves go.
            refresh(board, p, stack[cur].psq[p], stack[cur].tac[p]);
            stack[cur].computedTac[p] = true;
        }
        else
        {
            // Bucket only: tac indices depend on the mirror alone, so that half
            // stays reachable. 90% of bucket changes land here.
            refresh(board, p, stack[cur].psq[p], nullptr);
        }
        stack[cur].computedPsq[p] = true;
    }
}

void NNUE::updatePerspective(Board& board, Color perspective) const {
    auto&     stack = board.nnueData.accumulator;
    const int cur   = board.nnueData.size;

    const bool needPsq = !stack[cur].computedPsq[perspective];
    const bool needTac = !stack[cur].computedTac[perspective];
    if (!needPsq && !needTac)
        return;

    const PerspectiveKey key = perspectiveKey(bitScanForward(board.bitboards[pieceIndex(perspective, KING)]),
                                              perspective);

    int basePsq = -1;
    if (needPsq)
        for (int i = cur - 1; i >= 0; i--)
        {
            if (!stack[i].stateValid || perspectiveKey(stack[i].kingSq[perspective], perspective) != key)
                break;
            if (stack[i].computedPsq[perspective])
            {
                basePsq = i;
                break;
            }
        }

    int baseTac = -1;
    if (needTac)
        for (int i = cur - 1; i >= 0; i--)
        {
            if (!stack[i].stateValid
                || perspectiveKey(stack[i].kingSq[perspective], perspective).flip != key.flip)
                break;
            if (stack[i].computedTac[perspective])
            {
                baseTac = i;
                break;
            }
        }

    if (needPsq && basePsq < 0)
    {
        refresh(board, perspective, stack[cur].psq[perspective], nullptr);
        stack[cur].computedPsq[perspective] = true;
    }
    if (needTac && baseTac < 0)
    {
        refresh(board, perspective, nullptr, stack[cur].tac[perspective]);
        stack[cur].computedTac[perspective] = true;
    }

    // `cur + 1` means "this half needs no forward application".
    const int fromPsq = (needPsq && basePsq >= 0) ? basePsq + 1 : cur + 1;
    const int fromTac = (needTac && baseTac >= 0) ? baseTac + 1 : cur + 1;

    for (int i = std::min(fromPsq, fromTac); i <= cur; i++)
        applyPly(board, perspective, i, key, i >= fromPsq, i >= fromTac);
}

// --- Head -------------------------------------------------------------------

int NNUE::finishHead(const int32_t* l1Dots) const {
    const NetworkData& net = *network;

    // L1 feeds the head twice: through L2, and straight into the output layer.
    float l1Activations[2 * L1_SIZE];
    for (int i = 0; i < L1_SIZE; i++)
    {
        const float value          = static_cast<float>(l1Dots[i]) * net.l1Norm[i] + net.l1Biases[i];
        l1Activations[i]           = crelu(value);
        l1Activations[L1_SIZE + i] = screlu(value);
    }

    constexpr int F        = SIMD::fvecSize;
    constexpr int l2Vecs   = L2_SIZE / F;
    constexpr int headVecs = HEAD_SIZE / F;
    static_assert(L2_SIZE % F == 0 && HEAD_SIZE % F == 0, "the head must tile the float vectors");

    // L2, accumulated over inputs so the 32 outputs are one FMA per input
    // rather than 32 dependent reductions.
    SIMD::fvecType acc[l2Vecs];
    for (int j = 0; j < l2Vecs; j++)
        acc[j] = SIMD::fvecLoad(net.l2Biases + j * F);

    // Skipping zero activations only pays when the loop body is wide enough to
    // beat an unpredictable branch. On AVX-512 the body is two FMAs and the skip
    // measured 1.3% slower, so it is off there.
    constexpr bool skipZeros = l2Vecs >= 4;

    for (int c = 0; c < 2 * L1_SIZE; c++)
    {
        if (skipZeros && l1Activations[c] == 0.0f)
            continue;
        const SIMD::fvecType a = SIMD::fvecSet1(l1Activations[c]);
        const float*         w = net.l2Weights[c];
        for (int j = 0; j < l2Vecs; j++)
            acc[j] = SIMD::fvecFmadd(a, SIMD::fvecLoad(w + j * F), acc[j]);
    }

    const SIMD::fvecType zero = SIMD::fvecZero();
    const SIMD::fvecType one  = SIMD::fvecSet1(1.0f);

    alignas(64) float head[HEAD_SIZE];
    for (int j = 0; j < l2Vecs; j++)
    {
        const SIMD::fvecType clipped = SIMD::fvecMin(one, SIMD::fvecMax(zero, acc[j]));
        SIMD::fvecStore(head + j * F, SIMD::fvecMul(clipped, clipped));
    }
    std::memcpy(head + L2_SIZE, l1Activations, sizeof(l1Activations));

    // Adjacent-pair reduction, so the summation order does not depend on the
    // vector width.
    SIMD::fvecType products[headVecs];
    for (int k = 0; k < headVecs; k++)
        products[k] = SIMD::fvecMul(SIMD::fvecLoad(head + k * F), SIMD::fvecLoad(net.l3Weights + k * F));
    for (int width = headVecs; width > 1; width /= 2)
        for (int k = 0; k < width / 2; k++)
            products[k] = SIMD::fvecAdd(products[2 * k], products[2 * k + 1]);

    const float output = net.l3Biases + SIMD::fvecReduceAdd(products[0]);
    const int   score  = static_cast<int>(output * EVAL_SCALE);
    return std::clamp(score, -static_cast<int>(MAX_MATE_SCORE), static_cast<int>(MAX_MATE_SCORE));
}

void NNUE::computePairwise(const int16_t* stmPsq, const int16_t* stmTac, const int16_t* ntmPsq,
                           const int16_t* ntmTac, uint8_t* out) {
    const SIMD::vecType zero = SIMD::vecZero();
    const SIMD::vecType clip = SIMD::vecSet1Epi16(static_cast<int16_t>(QA));
    // TRUNCATE, do not round -- the trainer models it that way. Rounding reads
    // ~40 cp high on lopsided positions.
    for (int perspective = 0; perspective < 2; perspective++)
    {
        const int16_t* accPsq = perspective == 0 ? stmPsq : ntmPsq;
        const int16_t* accTac = perspective == 0 ? stmTac : ntmTac;
        uint8_t*       dest = out + perspective * PW;

        for (int i = 0; i < PW; i += 2 * SIMD::vecSize)
        {
            SIMD::vecType products[2];
            for (int half = 0; half < 2; half++)
            {
                const int           at = i + half * SIMD::vecSize;
                const SIMD::vecType rawLo = SIMD::vecAddEpi16(SIMD::vecLoad(accPsq + at), SIMD::vecLoad(accTac + at));
                const SIMD::vecType rawHi =
                    SIMD::vecAddEpi16(SIMD::vecLoad(accPsq + at + PW), SIMD::vecLoad(accTac + at + PW));
                const SIMD::vecType lo = SIMD::vecMinEpi16(clip, SIMD::vecMaxEpi16(zero, rawLo));
                const SIMD::vecType hi = SIMD::vecMinEpi16(clip, SIMD::vecMaxEpi16(zero, rawHi));
                products[half] = SIMD::vecSrliEpi16<INPUT_SHIFT>(SIMD::vecMulloEpi16(lo, hi));
            }
            SIMD::vecStoreRaw(dest + i, SIMD::packUnsignedEpi16(products[0], products[1]));
        }
    }
}

namespace {

// Set-bit positions per 8-bit mask, so the scan below is branchless: one
// unaligned store, then advance by the popcount.
alignas(64) constexpr auto NNZ_POSITIONS = [] {
    std::array<std::array<uint16_t, 8>, 256> table{};
    for (int mask = 0; mask < 256; mask++)
    {
        int n = 0;
        for (int bit = 0; bit < 8; bit++)
            if (mask & (1 << bit))
                table[mask][n++] = static_cast<uint16_t>(bit);
    }
    return table;
}();

}  // namespace

/// Sparse-input affine transform. Weights are input-group-major, so one 4-byte
/// input group times one weight vector is a slice of every neuron at once and
/// no horizontal reduction is needed. All-zero groups are skipped, which only
/// pays because the net is exported with co-quiet outputs grouped together
/// (tools/permute_net.py): 70% of groups live unordered, 54% ordered.
void NNUE::l1Dots(const uint8_t* pairwise, int32_t* dots) const {
    const NetworkData& net = *network;

    // 16 lanes on AVX-512, 8 on AVX2, 4 on SSE.
    constexpr int groupsPerVec = SIMD::vecSize / 2;
    constexpr int nAcc         = L1_SIZE / groupsPerVec;
    constexpr int maskBytes    = (groupsPerVec + 7) / 8;
    constexpr int unroll       = 4 / nAcc;
    static_assert(L1_SIZE % groupsPerVec == 0, "L1 must tile the accumulator vectors");

    uint16_t nonZero[L1_GROUPS + 8];
    int      count = 0;

    for (int group = 0; group < L1_GROUPS; group += groupsPerVec)
    {
        uint32_t mask = SIMD::nonZeroMaskEpi32(SIMD::vecLoadRaw(pairwise + 4 * group));
        for (int b = 0; b < maskBytes; b++)
        {
            const uint8_t byte = static_cast<uint8_t>(mask >> (8 * b));
            const __m128i base = _mm_set1_epi16(static_cast<int16_t>(group + 8 * b));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(nonZero + count),
                             _mm_add_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(NNZ_POSITIONS[byte].data())), base));
            count += __builtin_popcount(byte);
        }
    }

    SIMD::vecType acc[unroll][nAcc];
    for (int u = 0; u < unroll; u++)
        for (int n = 0; n < nAcc; n++)
            acc[u][n] = SIMD::vecZero();

    auto step = [&](int u, int g) {
        int32_t bytes;
        std::memcpy(&bytes, pairwise + 4 * g, sizeof(bytes));
        const SIMD::vecType inputs = SIMD::vecBroadcastEpi32(bytes);
        for (int n = 0; n < nAcc; n++)
            acc[u][n] = SIMD::vecDpbusdEpi32(acc[u][n], inputs,
                                             SIMD::vecLoadRaw(net.l1Weights[g] + 4 * groupsPerVec * n));
    };

    int i = 0;
    for (; i + unroll <= count; i += unroll)
        for (int u = 0; u < unroll; u++)
            step(u, nonZero[i + u]);
    for (; i < count; i++)
        step(0, nonZero[i]);

    for (int n = 0; n < nAcc; n++)
    {
        SIMD::vecType sum = acc[0][n];
        for (int u = 1; u < unroll; u++)
            sum = SIMD::vecAddEpi32(sum, acc[u][n]);
        SIMD::vecStoreEpi32(dots + groupsPerVec * n, sum);
    }
}

int NNUE::runHead(const int16_t* stmPsq, const int16_t* stmTac, const int16_t* ntmPsq,
                  const int16_t* ntmTac) const {
    alignas(64) uint8_t pairwise[2 * PW];
    alignas(64) int32_t dots[L1_SIZE];

    computePairwise(stmPsq, stmTac, ntmPsq, ntmTac, pairwise);
    l1Dots(pairwise, dots);
    return finishHead(dots);
}

void NNUE::runPolicyL1(const uint8_t* pairwise, uint8_t* hidden) const {
    const NetworkData& net = *network;

    // 16 lanes on AVX-512, 8 on AVX2, 4 on SSE.
    constexpr int groupsPerVec = SIMD::vecSize / 2;
    constexpr int maskBytes    = (groupsPerVec + 7) / 8;
    static_assert(POLICY_HIDDEN % groupsPerVec == 0, "Policy hidden must tile accumulator vectors");

    uint16_t nonZero[POLICY_L1_GROUPS + 8];
    int      count = 0;

    for (int group = 0; group < POLICY_L1_GROUPS; group += groupsPerVec)
    {
        uint32_t mask = SIMD::nonZeroMaskEpi32(SIMD::vecLoadRaw(pairwise + 4 * group));
        for (int b = 0; b < maskBytes; b++)
        {
            const uint8_t byte = static_cast<uint8_t>(mask >> (8 * b));
            const __m128i base = _mm_set1_epi16(static_cast<int16_t>(group + 8 * b));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(nonZero + count),
                             _mm_add_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(NNZ_POSITIONS[byte].data())), base));
            count += __builtin_popcount(byte);
        }
    }

    constexpr int TILE  = 256;
    constexpr int nVecs = TILE / groupsPerVec;

    for (int t = 0; t < POLICY_HIDDEN; t += TILE)
    {
        SIMD::vecType acc[nVecs];
        for (int v = 0; v < nVecs; v++)
            acc[v] = SIMD::vecZero();

        for (int i = 0; i < count; i++)
        {
            const int           g = nonZero[i];
            int32_t             bytes;
            std::memcpy(&bytes, pairwise + 4 * g, sizeof(bytes));
            const SIMD::vecType inputs = SIMD::vecBroadcastEpi32(bytes);
            for (int v = 0; v < nVecs; v++)
            {
                const int neuron = t + v * groupsPerVec;
                acc[v]           = SIMD::vecDpbusdEpi32(acc[v], inputs,
                                                        SIMD::vecLoadRaw(net.polL1Weights[g] + 4 * neuron));
            }
        }

        alignas(64) int32_t dots[TILE];
        for (int v = 0; v < nVecs; v++)
            SIMD::vecStoreEpi32(dots + v * groupsPerVec, acc[v]);

        for (int j = 0; j < TILE; j++)
        {
            const int   neuron = t + j;
            const float val    = static_cast<float>(dots[j]) * net.polL1Norm[neuron] + net.polL1Biases[neuron];
            hidden[neuron]     = static_cast<uint8_t>(std::clamp(static_cast<int>(val * 16.0f), 0, 127));
        }
    }
}

float NNUE::runPolicyL2(const uint8_t* hidden, int moveIdx) const {
    const NetworkData& net = *network;
    const int8_t*      w   = net.polL2Weights + static_cast<size_t>(moveIdx) * POLICY_HIDDEN;

    constexpr int bytesPerVec = SIMD::vecSize * 2;
    constexpr int nChunks     = POLICY_HIDDEN / bytesPerVec;

    SIMD::vecType acc = SIMD::vecZero();
    for (int c = 0; c < nChunks; c++)
    {
        const SIMD::vecType hVec = SIMD::vecLoadRaw(hidden + c * bytesPerVec);
        const SIMD::vecType wVec = SIMD::vecLoadRaw(w + c * bytesPerVec);
        acc                      = SIMD::vecDpbusdEpi32(acc, hVec, wVec);
    }

    const int32_t dot = SIMD::vecReduceAddEpi32(acc);
    return static_cast<float>(dot) * net.polL2Norm[moveIdx] + net.polL2Biases[moveIdx];
}

void NNUE::computePolicyHidden(Board& board, uint8_t* hidden) const {
    if (!policyLoaded)
        return;

    auto& acc = board.nnueData.accumulator[board.nnueData.size];
    if (!acc.stateValid)
    {
        acc.pawns[WHITE]  = board.bitboards[WHITE_PAWN];
        acc.pawns[BLACK]  = board.bitboards[BLACK_PAWN];
        acc.kingSq[WHITE] = static_cast<uint8_t>(bitScanForward(board.bitboards[WHITE_KING]));
        acc.kingSq[BLACK] = static_cast<uint8_t>(bitScanForward(board.bitboards[BLACK_KING]));
        acc.stateValid    = true;
    }

    updatePerspective(board, WHITE);
    updatePerspective(board, BLACK);

    const Color stm = board.sideToMove;
    const Color ntm = ~stm;
    alignas(64) uint8_t pairwise[2 * PW];
    computePairwise(acc.psq[stm], acc.tac[stm], acc.psq[ntm], acc.tac[ntm], pairwise);
    runPolicyL1(pairwise, hidden);
}

// --- Evaluation -------------------------------------------------------------

void NNUE::calculateInputLayer(Board& board, int idx, bool fromScratch) {
    if (!loaded)
        return;

    auto& acc = board.nnueData.accumulator[idx];

    if (fromScratch || !acc.computedPsq[WHITE] || !acc.computedPsq[BLACK] || !acc.computedTac[WHITE]
        || !acc.computedTac[BLACK])
    {
        refresh(board, WHITE, acc.psq[WHITE], acc.tac[WHITE]);
        refresh(board, BLACK, acc.psq[BLACK], acc.tac[BLACK]);
        acc.computedPsq[WHITE] = acc.computedPsq[BLACK] = true;
        acc.computedTac[WHITE] = acc.computedTac[BLACK] = true;
    }

    acc.pawns[WHITE]  = board.bitboards[WHITE_PAWN];
    acc.pawns[BLACK]  = board.bitboards[BLACK_PAWN];
    acc.kingSq[WHITE] = static_cast<uint8_t>(bitScanForward(board.bitboards[WHITE_KING]));
    acc.kingSq[BLACK] = static_cast<uint8_t>(bitScanForward(board.bitboards[BLACK_KING]));
    acc.stateValid    = true;
}

int NNUE::evaluate(Board& board) {
    if (!loaded)
        return 0;

    auto& acc = board.nnueData.accumulator[board.nnueData.size];

    if (!acc.stateValid)
    {
        acc.pawns[WHITE]  = board.bitboards[WHITE_PAWN];
        acc.pawns[BLACK]  = board.bitboards[BLACK_PAWN];
        acc.kingSq[WHITE] = static_cast<uint8_t>(bitScanForward(board.bitboards[WHITE_KING]));
        acc.kingSq[BLACK] = static_cast<uint8_t>(bitScanForward(board.bitboards[BLACK_KING]));
        acc.stateValid    = true;
    }

    updatePerspective(board, WHITE);
    updatePerspective(board, BLACK);

    const Color stm = board.sideToMove;
    const Color ntm = ~stm;
    return runHead(acc.psq[stm], acc.tac[stm], acc.psq[ntm], acc.tac[ntm]);
}

float NNUE::halfMoveScale(Board& board) { return (100.0f - board.halfMove) / 100.0f; }

float NNUE::materialScale(Board& board) {
    float gamePhase = popcount64(board.bitboards[WHITE_KNIGHT] | board.bitboards[BLACK_KNIGHT]
                                 | board.bitboards[WHITE_BISHOP] | board.bitboards[BLACK_BISHOP])
                    * 3.0f;
    gamePhase += popcount64(board.bitboards[WHITE_ROOK] | board.bitboards[BLACK_ROOK]) * 5.0f;
    gamePhase += popcount64(board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN]) * 10.0f;

    gamePhase = std::min(gamePhase, 64.0f);

    constexpr float a = 0.8f;
    constexpr float b = 1.0f;

    return a + (b - a) * gamePhase / 64.0f;
}

// --- Loading ----------------------------------------------------------------

bool NNUE::loadFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel) {
    auto fail = [&](const std::string& message) {
        std::cout << "info string Failed to load net from " << sourceLabel << ": " << message << std::endl;
        loaded = false;
        return false;
    };

    if (!data || size < HEADER_LEN)
        return fail("shorter than the header");

    if (std::memcmp(data, MAGIC, sizeof(MAGIC)) != 0)
        return fail("bad magic (expected DVNNUE5)");

    auto expect = [&](const char* field, uint64_t got, uint64_t want, bool& ok) {
        if (got != want)
        {
            std::cout << "info string Network " << field << " is " << got << ", engine expects " << want << std::endl;
            ok = false;
        }
    };

    bool ok = true;
    const uint32_t rawVersion = readU32(data, 8);
    const uint16_t version    = rawVersion & 0xFFFF;
    const uint16_t flags      = rawVersion >> 16;
    expect("version", version, NET_VERSION, ok);
    expect("kingBuckets", readU32(data, 12), KING_BUCKETS, ok);
    expect("ftSize", readU32(data, 16), NNUE_FT_OUT, ok);
    expect("outputBuckets", readU32(data, 20), OUTPUT_BUCKETS, ok);
    expect("pawnPairs", readU32(data, 24), PAWN_PAIRS, ok);
    expect("psqFeatures", readU32(data, 28), PSQ_FEATURES, ok);
    expect("inputQuant", readU32(data, 32), QA, ok);
    expect("inputShift", readU32(data, 36), INPUT_SHIFT, ok);
    expect("l1Size", readU32(data, 40), L1_SIZE, ok);
    expect("l2Size", readU32(data, 44), L2_SIZE, ok);
    if (!ok)
        return fail("architecture mismatch");

    constexpr size_t payload = sizeof(NetworkData::ftPsqWeights) + sizeof(NetworkData::ftTacWeights)
                             + sizeof(NetworkData::ftTacBiases)
                             + sizeof(NetworkData::l1Weights) + sizeof(NetworkData::l1Norm)
                             + sizeof(NetworkData::l1Biases)
                             + sizeof(NetworkData::l2Weights) + sizeof(NetworkData::l2Biases)
                             + sizeof(NetworkData::l3Weights) + sizeof(NetworkData::l3Biases);

    if (size - HEADER_LEN < payload)
        return fail("payload is " + std::to_string(size - HEADER_LEN) + " bytes, architecture needs "
                    + std::to_string(payload));

    if (!network)
        network = std::make_unique<NetworkData>();
    NetworkData& net = *network;

    const uint8_t* src  = data + HEADER_LEN;
    auto           read = [&src](void* dst, size_t bytes) {
        std::memcpy(dst, src, bytes);
        src += bytes;
    };

    read(net.ftPsqWeights, sizeof(net.ftPsqWeights));
    read(net.ftTacWeights, sizeof(net.ftTacWeights));
    read(net.ftTacBiases, sizeof(net.ftTacBiases));

    // L1 arrives NEURON-major and is transposed to the input-group-major layout
    // l1Dots consumes; see NetworkData::l1Weights.
    {
        std::vector<int8_t> raw(static_cast<size_t>(L1_SIZE) * 2 * PW);
        read(raw.data(), raw.size());
        for (int neuron = 0; neuron < L1_SIZE; neuron++)
            for (int in = 0; in < 2 * PW; in++)
                net.l1Weights[in / 4][4 * neuron + in % 4] = raw[static_cast<size_t>(neuron) * 2 * PW + in];
    }

    read(net.l1Norm, sizeof(net.l1Norm));
    read(net.l1Biases, sizeof(net.l1Biases));
    read(net.l2Weights, sizeof(net.l2Weights));
    read(net.l2Biases, sizeof(net.l2Biases));
    read(net.l3Weights, sizeof(net.l3Weights));
    read(&net.l3Biases, sizeof(net.l3Biases));

    // File is output-major; transpose to input-major so finishHead accumulates
    // over inputs (one vector FMA per input, no serial reduction).
    {
        float raw[L2_SIZE][2 * L1_SIZE];
        std::memcpy(raw, net.l2Weights, sizeof(raw));
        for (int out = 0; out < L2_SIZE; out++)
            for (int in = 0; in < 2 * L1_SIZE; in++)
                net.l2Weights[in][out] = raw[out][in];
    }

    constexpr size_t polPayload = sizeof(NetworkData::polL1Weights) + sizeof(NetworkData::polL1Norm)
                                + sizeof(NetworkData::polL1Biases) + sizeof(NetworkData::polL2Weights)
                                + sizeof(NetworkData::polL2Norm) + sizeof(NetworkData::polL2Biases);

    const bool hasPolicyFlag = (flags & 1) != 0;
    if (hasPolicyFlag && (size - HEADER_LEN >= payload + polPayload))
    {
        read(net.polL1Weights, sizeof(net.polL1Weights));
        read(net.polL1Norm, sizeof(net.polL1Norm));
        read(net.polL1Biases, sizeof(net.polL1Biases));
        read(net.polL2Weights, sizeof(net.polL2Weights));
        read(net.polL2Norm, sizeof(net.polL2Norm));
        read(net.polL2Biases, sizeof(net.polL2Biases));
        policyLoaded = true;
    }
    else
    {
        policyLoaded = false;
    }

    loaded = true;

    std::cout << "info string Loaded net from " << sourceLabel << " (" << FT_IN << " -> " << NNUE_FT_OUT
              << ", " << KING_BUCKETS << " king buckets, " << NUM_TAC_FEATURES << " tactical features, " << OUTPUT_BUCKETS
              << " output buckets" << (policyLoaded ? ", with policy head)" : ")") << std::endl;
    return true;
}

bool NNUE::loadNetwork(const std::string& filePath) {
    if (filePath.empty() || filePath == "<empty>")
        return false;

    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cout << "info string Failed to open network file: " << filePath << std::endl;
        return false;
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0)
    {
        std::cout << "info string Failed to read network file: " << filePath << std::endl;
        return false;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(fileSize));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), fileSize))
    {
        std::cout << "info string Failed to read network file: " << filePath << std::endl;
        return false;
    }

    return loadFromBuffer(bytes.data(), bytes.size(), filePath);
}

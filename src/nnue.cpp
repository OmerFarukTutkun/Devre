#include "nnue.h"
#include "simd.h"
#include "util.h"
#include "incbin/incbin.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>

#ifndef NET
    #define NET "quantised.bin"
#endif

INCBIN(EmbeddedNet, NET);

namespace {

constexpr int EVAL_SCALE = 400;

// The pairwise stage lands at QA * QA >> INPUT_SHIFT = 126.008, not QA.
constexpr float PAIRWISE_QUANT   = static_cast<float>(NNUE::QA) * NNUE::QA / (1 << NNUE::INPUT_SHIFT);
constexpr int   PAIRWISE_MAX     = (NNUE::QA * NNUE::QA) >> NNUE::INPUT_SHIFT;
static_assert(PAIRWISE_MAX == 126, "the pairwise activations must fit in int8");

static_assert(2 * PAIRWISE_MAX * 127 <= 32767, "maddubs would saturate");

// int16 accumulator: the bias plus every one of the 32 psq and 96 pawn-pair rows
// at the int8 cap, all in one cell.
static_assert(NNUE::QA + (32 + 96) * 127 <= 32767, "the accumulator would overflow");

// clang-format off
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

// --- Accumulator arithmetic -------------------------------------------------

constexpr uint32_t numBlocks = NNUE_FT_OUT / SIMD::vecSize;
static_assert(NNUE_FT_OUT % SIMD::vecSize == 0);

/// `out = base + sum(addRows) - sum(subRows)` in one pass.
///
/// `out` may alias `base` -- the refresh cache updates in place -- so nothing is
/// `restrict`. Each chunk loads before it stores, so aliasing is safe.
inline __attribute__((always_inline)) void
accumulateRows(int16_t* out, const int16_t* base, const int8_t* const* addRows, int nAdd,
               const int8_t* const* subRows, int nSub) {
    constexpr uint32_t chunkBlocks = (numBlocks < 8) ? numBlocks : 8;

    for (uint32_t chunk = 0; chunk < numBlocks; chunk += chunkBlocks)
    {
        const uint32_t offset = chunk * SIMD::vecSize;

        SIMD::vecType acc[chunkBlocks];
        for (uint32_t b = 0; b < chunkBlocks; b++)
            acc[b] = SIMD::vecLoad(base + offset + b * SIMD::vecSize);

        for (int a = 0; a < nAdd; a++)
        {
            const int8_t* row = addRows[a] + offset;
            for (uint32_t b = 0; b < chunkBlocks; b++)
                acc[b] = SIMD::vecAddEpi16(acc[b], SIMD::vecLoadI8ToI16(row + b * SIMD::vecSize));
        }

        for (int s = 0; s < nSub; s++)
        {
            const int8_t* row = subRows[s] + offset;
            for (uint32_t b = 0; b < chunkBlocks; b++)
                acc[b] = SIMD::vecSubEpi16(acc[b], SIMD::vecLoadI8ToI16(row + b * SIMD::vecSize));
        }

        for (uint32_t b = 0; b < chunkBlocks; b++)
            SIMD::vecStore(out + offset + b * SIMD::vecSize, acc[b]);
    }
}

inline float crelu(float x) { return std::clamp(x, 0.0f, 1.0f); }

inline float screlu(float x) {
    const float c = crelu(x);
    return c * c;
}

// --- File format ------------------------------------------------------------

// 48-byte header (magic, format version, nine architecture fields) then the
// tensors back to back in NetworkData's declaration order. `VERSION` is already
// the engine's own version macro, hence the prefix.
constexpr char     MAGIC[8]    = {'D', 'V', 'N', 'N', 'U', 'E', '4', '\0'};
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

int NNUE::outputBucket(const Board& board) {
    const int pieces = popcount64(board.occupied[WHITE] | board.occupied[BLACK]);
    return std::min((pieces - 2) / 4, OUTPUT_BUCKETS - 1);
}

// --- Full refresh -----------------------------------------------------------

void NNUE::refresh(const Board& board, Color perspective, int16_t* accumulator) const {
    const NetworkData&   net = *network;
    const PerspectiveKey key = perspectiveKey(bitScanForward(board.bitboards[pieceIndex(perspective, KING)]),
                                              perspective);

    const int8_t* rows[32 + 96];
    int           nRows = 0;

    // Entry i of both buffers must be the same pawn: pawnPairIndex
    // canonicalises each perspective independently.
    int pawnIds[16];
    int pawnFiles[16];
    int pawns = 0;

    uint64_t occ = board.occupied[WHITE] | board.occupied[BLACK];
    while (occ)
    {
        const int sq    = poplsb(occ);
        const int piece = board.pieceBoard[sq];

        rows[nRows++] = net.ftWeights + static_cast<size_t>(psqFeature(piece, sq, perspective, key)) * NNUE_FT_OUT;

        if (pieceType(piece) == PAWN)
        {
            pawnIds[pawns] = pawnId(sq, static_cast<Color>(pieceColor(piece)), perspective, key.flip);
            pawnFiles[pawns] = sq & 7;
            pawns++;
        }
    }

    for (int i = 0; i < pawns; i++)
        for (int j = i + 1; j < pawns; j++)
            if (std::abs(pawnFiles[i] - pawnFiles[j]) <= 1)
                rows[nRows++] = net.ftWeights
                              + static_cast<size_t>(PSQ_FEATURES + pawnPairIndex(pawnIds[i], pawnIds[j]))
                                    * NNUE_FT_OUT;

    accumulateRows(accumulator, net.ftBiases, rows, nRows, nullptr, 0);
}

// --- Incremental update -----------------------------------------------------

/// Apply ply `idx` on top of ply `idx - 1`: psq from the change list, pawn
/// pairs from the pawn bitboards either side of the move.
void NNUE::applyPly(Board& board, Color perspective, int idx, PerspectiveKey key) const {
    const NetworkData& net  = *network;
    auto&              cur  = board.nnueData.accumulator[idx];
    const auto&        prev = board.nnueData.accumulator[idx - 1];

    // 4 psq + 2 moved pawns x 15 partners + 1 = 35.
    const int8_t* addRows[48];
    const int8_t* subRows[48];
    int            nAdd = 0;
    int            nSub = 0;

    auto row = [&](int feature) { return net.ftWeights + static_cast<size_t>(feature) * NNUE_FT_OUT; };

    for (int i = 0; i < cur.changeCount; i++)
    {
        const auto& change  = cur.changes[i];
        const int   feature = psqFeature(change.piece, change.sq, perspective, key);
        if (change.sign > 0)
            addRows[nAdd++] = row(feature);
        else
            subRows[nSub++] = row(feature);
    }

    pawnPairDelta(prev.pawns, cur.pawns, perspective, key, addRows, nAdd, subRows, nSub);


    accumulateRows(cur.data[perspective], prev.data[perspective], addRows, nAdd, subRows, nSub);
    cur.computed[perspective] = true;
}

void NNUE::pawnPairDelta(const uint64_t prevPawns[N_COLORS], const uint64_t curPawns[N_COLORS], Color perspective,
                         PerspectiveKey key, const int8_t** addRows, int& nAdd, const int8_t** subRows,
                         int& nSub) const {
    if (prevPawns[WHITE] == curPawns[WHITE] && prevPawns[BLACK] == curPawns[BLACK])
        return;

    const NetworkData& net = *network;
    auto row = [&](int feature) { return net.ftWeights + static_cast<size_t>(feature) * NNUE_FT_OUT; };

    // id in the low byte, file above it: recovering the file from the id would
    // have to undo the mirror. A refresh can change all sixteen pawns.
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
                rows[n++]    = row(PSQ_FEATURES + pawnPairIndex(id, pawnId(sq, WHITE, perspective, key.flip)));
            }

            partners = keptBlack & ADJACENT_FILES[file];
            while (partners)
            {
                const int sq = poplsb(partners);
                rows[n++]    = row(PSQ_FEATURES + pawnPairIndex(id, pawnId(sq, BLACK, perspective, key.flip)));
            }

            for (int j = i + 1; j < count; j++)
                if (std::abs(file - (moved[j] >> 8)) <= 1)
                    rows[n++] = row(PSQ_FEATURES + pawnPairIndex(id, moved[j] & 0xFF));
        }
    };

    pairsWithKept(removed, nRemoved, subRows, nSub);
    pairsWithKept(added, nAdded, addRows, nAdd);
}

/// Rebuild a perspective as a delta from the last position that used the same
/// bucket and mirror, rather than from the bias. An empty entry degenerates to
/// the bias rebuild, hence the full-rebuild row bound below.
void NNUE::refreshFromCache(Board& board, Color perspective, PerspectiveKey key, int16_t* accumulator) const {
    const NetworkData& net   = *network;
    auto&              entry = board.nnueData.refreshCache[(perspective * KING_BUCKETS + key.bucket) * 2
                                              + (key.flip & 7 ? 1 : 0)];

    if (!entry.initialised)
    {
        std::memcpy(entry.data, net.ftBiases, sizeof(net.ftBiases));
        std::memset(entry.pieces, 0, sizeof(entry.pieces));
        entry.initialised = true;
    }

    // 32 psq + at most 96 pawn pairs; no delta exceeds a full rebuild.
    const int8_t* addRows[32 + 96];
    const int8_t* subRows[32 + 96];
    int            nAdd = 0;
    int            nSub = 0;

    auto row = [&](int feature) { return net.ftWeights + static_cast<size_t>(feature) * NNUE_FT_OUT; };

    for (int piece = 0; piece < N_PIECES; piece++)
    {
        if (pieceType(piece) > KING)
            continue;

        uint64_t gone = entry.pieces[piece] & ~board.bitboards[piece];
        while (gone)
            subRows[nSub++] = row(psqFeature(piece, poplsb(gone), perspective, key));

        uint64_t fresh = board.bitboards[piece] & ~entry.pieces[piece];
        while (fresh)
            addRows[nAdd++] = row(psqFeature(piece, poplsb(fresh), perspective, key));
    }

    const uint64_t cachedPawns[N_COLORS]  = {entry.pieces[WHITE_PAWN], entry.pieces[BLACK_PAWN]};
    const uint64_t currentPawns[N_COLORS] = {board.bitboards[WHITE_PAWN], board.bitboards[BLACK_PAWN]};
    pawnPairDelta(cachedPawns, currentPawns, perspective, key, addRows, nAdd, subRows, nSub);


    accumulateRows(entry.data, entry.data, addRows, nAdd, subRows, nSub);
    std::memcpy(entry.pieces, board.bitboards, sizeof(entry.pieces));
    std::memcpy(accumulator, entry.data, NNUE_FT_OUT * sizeof(int16_t));
}

void NNUE::updatePerspective(Board& board, Color perspective) const {
    auto&     stack = board.nnueData.accumulator;
    const int cur   = board.nnueData.size;

    if (stack[cur].computed[perspective])
        return;

    const PerspectiveKey key = perspectiveKey(bitScanForward(board.bitboards[pieceIndex(perspective, KING)]),
                                              perspective);

    // Nearest computed accumulator with the same key: a bucket or mirror change
    // re-indexes every psq row and cannot be bridged with deltas.
    int base = -1;
    for (int i = cur - 1; i >= 0; i--)
    {
        if (!stack[i].stateValid || perspectiveKey(stack[i].kingSq[perspective], perspective) != key)
            break;
        if (stack[i].computed[perspective])
        {
            base = i;
            break;
        }
    }

    if (base < 0)
    {
        refreshFromCache(board, perspective, key, stack[cur].data[perspective]);
        stack[cur].computed[perspective] = true;
        return;
    }

    for (int i = base + 1; i <= cur; i++)
        applyPly(board, perspective, i, key);
}

// --- Head -------------------------------------------------------------------

/// L1 dot products are already done, in int32.
int NNUE::finishHead(const int32_t* l1Dots, int bucket) const {
    const NetworkData& net = *network;

    float l1Activations[2 * L1_SIZE];
    for (int i = 0; i < L1_SIZE; i++)
    {
        const int   out   = bucket * L1_SIZE + i;
        const float value = static_cast<float>(l1Dots[i]) * net.l1Norm[out] + net.l1Biases[out];
        l1Activations[i]           = crelu(value);
        l1Activations[L1_SIZE + i] = screlu(value);
    }

    // Across outputs, not within one: the weights are input-major, so each
    // input is one contiguous FMA over all 32 outputs and nothing reduces.
    alignas(64) float head[HEAD_SIZE];
    std::memcpy(head, net.l2Biases + bucket * L2_SIZE, L2_SIZE * sizeof(float));

    for (int c = 0; c < 2 * L1_SIZE; c++)
    {
        const float  x = l1Activations[c];
        const float* w = net.l2Weights[bucket][c];
        for (int j = 0; j < L2_SIZE; j++)
            head[j] += x * w[j];
    }

    for (int j = 0; j < L2_SIZE; j++)
        head[j] = screlu(head[j]);

    // Skip connection: the output layer sees the L1 activations as well.
    std::memcpy(head + L2_SIZE, l1Activations, sizeof(l1Activations));

    
    constexpr int LANES = 8;
    static_assert(HEAD_SIZE % LANES == 0);

    float partial[LANES] = {};
    for (int c = 0; c < HEAD_SIZE; c += LANES)
        for (int lane = 0; lane < LANES; lane++)
            partial[lane] += head[c + lane] * net.l3Weights[bucket][c + lane];

    float output = net.l3Biases[bucket];
    for (int lane = 0; lane < LANES; lane++)
        output += partial[lane];

    const int score = static_cast<int>(output * EVAL_SCALE);
    return std::clamp(score, -static_cast<int>(MAX_MATE_SCORE), static_cast<int>(MAX_MATE_SCORE));
}

/// Pairwise, side to move first, straight into the uint8 layout L1 wants. The
/// product of two [0, 255] values fits a 16-bit lane, so a logical shift is the
/// whole `>> INPUT_SHIFT` and nothing widens to int32.
void NNUE::computePairwise(const int16_t* stm, const int16_t* nstm, uint8_t* out) {
    const SIMD::vecType zero = SIMD::vecZero();
    const SIMD::vecType clip = SIMD::vecSet1Epi16(static_cast<int16_t>(QA));

    for (int perspective = 0; perspective < 2; perspective++)
    {
        const int16_t* acc  = perspective == 0 ? stm : nstm;
        uint8_t*       dest = out + perspective * PW;

        for (int i = 0; i < PW; i += 2 * SIMD::vecSize)
        {
            SIMD::vecType products[2];
            for (int half = 0; half < 2; half++)
            {
                const int           at = i + half * SIMD::vecSize;
                const SIMD::vecType lo = SIMD::vecMinEpi16(clip, SIMD::vecMaxEpi16(zero, SIMD::vecLoad(acc + at)));
                const SIMD::vecType hi =
                    SIMD::vecMinEpi16(clip, SIMD::vecMaxEpi16(zero, SIMD::vecLoad(acc + at + PW)));
                products[half] = SIMD::vecSrliEpi16<INPUT_SHIFT>(SIMD::vecMulloEpi16(lo, hi));
            }
            SIMD::vecStoreRaw(dest + i, SIMD::packUnsignedEpi16(products[0], products[1]));
        }
    }
}

/// Eight output neurons at a time: one reduction tree, shared input loads.
void NNUE::l1Dots(const uint8_t* pairwise, int bucket, int32_t* dots) const {
    const NetworkData&  net  = *network;
    const SIMD::vecType ones = SIMD::vecSet1Epi16(1);

    for (int group = 0; group < L1_SIZE; group += 8)
    {
        SIMD::vecType sums[8];
        for (int n = 0; n < 8; n++)
            sums[n] = SIMD::vecZero();

        for (int c = 0; c < 2 * PW; c += SIMD::vecSize * 2)
        {
            const SIMD::vecType inputs = SIMD::vecLoadRaw(pairwise + c);
            for (int n = 0; n < 8; n++)
            {
                const SIMD::vecType w = SIMD::vecLoadRaw(net.l1Weights[bucket * L1_SIZE + group + n] + c);
                sums[n] = SIMD::vecAddEpi32(sums[n], SIMD::vecMaddEpi16(SIMD::vecMaddubsEpi16(inputs, w), ones));
            }
        }

        SIMD::vecReduceEpi32x8(sums[0], sums[1], sums[2], sums[3], sums[4], sums[5], sums[6], sums[7], dots + group);
    }
}

int NNUE::runHead(const int16_t* stm, const int16_t* nstm, int bucket) const {
    alignas(64) uint8_t pairwise[2 * PW];
    alignas(64) int32_t dots[L1_SIZE];

    computePairwise(stm, nstm, pairwise);
    l1Dots(pairwise, bucket, dots);
    return finishHead(dots, bucket);
}

// --- Evaluation -------------------------------------------------------------

void NNUE::calculateInputLayer(Board& board, int idx, bool fromScratch) {
    if (!loaded)
        return;

    auto& acc = board.nnueData.accumulator[idx];

    if (fromScratch || !acc.computed[WHITE] || !acc.computed[BLACK])
    {
        refresh(board, WHITE, acc.data[WHITE]);
        refresh(board, BLACK, acc.data[BLACK]);
        acc.computed[WHITE] = acc.computed[BLACK] = true;
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

    // makeMove fills this; it is missing only at a root set up without one.
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

    return runHead(acc.data[board.sideToMove], acc.data[~board.sideToMove], outputBucket(board));
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
        return fail("bad magic (expected DVNNUE3)");

    auto expect = [&](const char* field, uint64_t got, uint64_t want, bool& ok) {
        if (got != want)
        {
            std::cout << "info string Network " << field << " is " << got << ", engine expects " << want << std::endl;
            ok = false;
        }
    };

    bool ok = true;
    expect("version", readU32(data, 8), NET_VERSION, ok);
    expect("kingBuckets", readU32(data, 12), KING_BUCKETS, ok);
    expect("ftSize", readU32(data, 16), NNUE_FT_OUT, ok);
    expect("outputBuckets", readU32(data, 20), OUTPUT_BUCKETS, ok);
    expect("pawnPairs", readU32(data, 24), PAWN_PAIRS, ok);
    expect("psqFeatures", readU32(data, 28), PSQ_FEATURES, ok);
    expect("inputQuant", readU32(data, 32), QA, ok);
    expect("l1Quant", readU32(data, 36), L1_QUANT, ok);
    expect("l1Size", readU32(data, 40), L1_SIZE, ok);
    expect("l2Size", readU32(data, 44), L2_SIZE, ok);
    if (!ok)
        return fail("architecture mismatch");

    // Not sizeof(NetworkData): the struct is padded to its alignment, the file
    // is not.
    constexpr size_t payload = sizeof(NetworkData::ftWeights) + sizeof(NetworkData::ftBiases)
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

    read(net.ftWeights, sizeof(net.ftWeights));
    read(net.ftBiases, sizeof(net.ftBiases));
    read(net.l1Weights, sizeof(net.l1Weights));
    read(net.l1Norm, sizeof(net.l1Norm));
    read(net.l1Biases, sizeof(net.l1Biases));
    read(net.l2Weights, sizeof(net.l2Weights));
    read(net.l2Biases, sizeof(net.l2Biases));
    read(net.l3Weights, sizeof(net.l3Weights));
    read(net.l3Biases, sizeof(net.l3Biases));

    // L2 arrives output-major, evaluated input-major. Same bytes, so transpose
    // in place.
    for (int b = 0; b < OUTPUT_BUCKETS; b++)
    {
        float raw[L2_SIZE][2 * L1_SIZE];
        std::memcpy(raw, net.l2Weights[b], sizeof(raw));

        for (int out = 0; out < L2_SIZE; out++)
            for (int in = 0; in < 2 * L1_SIZE; in++)
                net.l2Weights[b][in][out] = raw[out][in];
    }

    loaded = true;

    std::cout << "info string Loaded net from " << sourceLabel << " (" << NNUE_FT_IN << " -> " << NNUE_FT_OUT
              << ", " << KING_BUCKETS << " king buckets, " << PAWN_PAIRS << " pawn pairs, " << OUTPUT_BUCKETS
              << " output buckets)" << std::endl;
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

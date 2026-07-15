#include "nnue.h"
#include "simd.h"
#include "incbin/incbin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>

#ifndef NET
    #define NET "quantised.bin"
#endif

INCBIN(EmbeddedNet, NET);

namespace {
constexpr int QA    = 255;
constexpr int QB    = 64;
constexpr int SCALE = 300;

constexpr uint32_t numBlocks = NNUE_FT_OUT / SIMD::vecSize;
static_assert(NNUE_FT_OUT % SIMD::vecSize == 0);

inline __attribute__((always_inline)) void
applyDelta(int16_t* __restrict__ out, const int16_t* __restrict__ prev,
           const int16_t* const* addRows, int nAdd,
           const int16_t* const* subRows, int nSub) {
    constexpr uint32_t chunkBlocks = 8;
    for (uint32_t chunk = 0; chunk < numBlocks; chunk += chunkBlocks) {
        const uint32_t base = chunk * SIMD::vecSize;

        SIMD::vecType acc[chunkBlocks];
        for (uint32_t b = 0; b < chunkBlocks; b++)
            acc[b] = SIMD::vecLoad(prev + base + b * SIMD::vecSize);

        for (int a = 0; a < nAdd; a++) {
            const int16_t* row = addRows[a] + base;
            for (uint32_t b = 0; b < chunkBlocks; b++)
                acc[b] = SIMD::vecAddEpi16(acc[b], SIMD::vecLoad(row + b * SIMD::vecSize));
        }

        for (int s = 0; s < nSub; s++) {
            const int16_t* row = subRows[s] + base;
            for (uint32_t b = 0; b < chunkBlocks; b++)
                acc[b] = SIMD::vecSubEpi16(acc[b], SIMD::vecLoad(row + b * SIMD::vecSize));
        }

        for (uint32_t b = 0; b < chunkBlocks; b++)
            SIMD::vecStore(out + base + b * SIMD::vecSize, acc[b]);
    }
}
}

NNUE NNUE::instance;

NNUE::NNUE() {
    if (!loadNetFromBuffer(gEmbeddedNetData, gEmbeddedNetSize, "<embedded>"))
        std::cout << "info string Failed to load embedded net" << std::endl;
}

int NNUE::baseIndex(int piece, int sq, Color perspective) {
    const int orientedSquare = (perspective == BLACK) ? mirrorVertically(sq) : sq;
    const int pIdx           = pieceType(piece) + 6 * (pieceColor(piece) != perspective);
    return orientedSquare + pIdx * 64;
}

void NNUE::recalculateInputLayer(Board& board, Color perspective, int idx) {
    auto& acc = board.nnueData.accumulator[idx];
    int16_t* out = acc.data[perspective];

    const int16_t* addRows[N_SQUARES];
    int nAdd = 0;
    for (int sq = 0; sq < N_SQUARES; sq++)
    {
        const int piece = board.pieceBoard[sq];
        if (piece == EMPTY)
            continue;

        const int feature = baseIndex(piece, sq, perspective);
        addRows[nAdd++] = ftWeights + feature * NNUE_FT_OUT;
    }

    applyDelta(out, ftBiases, addRows, nAdd, nullptr, 0);
}

void NNUE::updateInputLayer(Board& board, int idx, bool fromScratch) {
    if (idx == 0 || fromScratch || !board.nnueData.accumulator[idx - 1].nonEmpty)
    {
        recalculateInputLayer(board, WHITE, idx);
        recalculateInputLayer(board, BLACK, idx);
    }
    else
    {
        auto& curAcc = board.nnueData.accumulator[idx];
        auto& prevAcc = board.nnueData.accumulator[idx - 1];

        for (int perspective = 0; perspective < N_COLORS; perspective++)
        {
            int16_t* cur = curAcc.data[perspective];
            const int16_t* prev = prevAcc.data[perspective];

            if (curAcc.changeCount == 0)
            {
                std::memcpy(cur, prev, NNUE_FT_OUT * sizeof(int16_t));
            }
            else
            {
                const int16_t* addRows[4];
                const int16_t* subRows[4];
                int nAdd = 0;
                int nSub = 0;

                for (int i = 0; i < curAcc.changeCount; i++)
                {
                    const auto& change = curAcc.changes[i];
                    const int feature = baseIndex(change.piece, change.sq, static_cast<Color>(perspective));
                    if (change.sign > 0)
                        addRows[nAdd++] = ftWeights + feature * NNUE_FT_OUT;
                    else
                        subRows[nSub++] = ftWeights + feature * NNUE_FT_OUT;
                }

                applyDelta(cur, prev, addRows, nAdd, subRows, nSub);
            }
        }
    }
    board.nnueData.accumulator[idx].nonEmpty = true;
}

void NNUE::calculateInputLayer(Board& board, int idx, bool fromScratch) {
    updateInputLayer(board, idx, fromScratch);
}

int NNUE::head(const int16_t* us, const int16_t* them) const {
    const SIMD::vecType zero = SIMD::vecZero();
    const SIMD::vecType clip = SIMD::vecSet1Epi16(static_cast<int16_t>(QA));
    SIMD::vecType sum = SIMD::vecZero();

    // SCReLU: screlu(x) = clamp(x, 0, QA)^2
    // SIMD trick: madd(clamped, mullo(clamped, weight)) computes sum of clamped[i]^2 * weight[i]
    // Safe because clamped ∈ [0,255] and weight fits i16, so mullo stays in i16 range.
    for (uint32_t k = 0; k < NNUE_FT_OUT; k += SIMD::vecSize)
    {
        SIMD::vecType u = SIMD::vecMinEpi16(clip, SIMD::vecMaxEpi16(zero, SIMD::vecLoad(us + k)));
        SIMD::vecType w = SIMD::vecLoad(headWeights + k);
        sum = SIMD::vecAddEpi32(sum, SIMD::vecMaddEpi16(u, SIMD::vecMulloEpi16(u, w)));
    }

    for (uint32_t k = 0; k < NNUE_FT_OUT; k += SIMD::vecSize)
    {
        SIMD::vecType t = SIMD::vecMinEpi16(clip, SIMD::vecMaxEpi16(zero, SIMD::vecLoad(them + k)));
        SIMD::vecType w = SIMD::vecLoad(headWeights + NNUE_FT_OUT + k);
        sum = SIMD::vecAddEpi32(sum, SIMD::vecMaddEpi16(t, SIMD::vecMulloEpi16(t, w)));
    }

    // De-quantise: QA*QA*QB → QA*QB
    int32_t output = SIMD::vecReduceEpi32(sum);
    output /= QA;

    // Add bias (already at QA*QB scale)
    output += static_cast<int32_t>(headBias);

    // Apply eval scale and remove remaining quantisation
    output *= SCALE;
    output /= QA * QB;

    return std::clamp(output, -static_cast<int32_t>(MAX_MATE_SCORE),
                              static_cast<int32_t>(MAX_MATE_SCORE));
}

int NNUE::evaluateNet(Board& board) {
    if (!loaded)
        return 0;

    int lastNonEmptyAcc = -1;
    for (int i = board.nnueData.size; i >= 0; i--)
    {
        if (board.nnueData.accumulator[i].nonEmpty)
        {
            lastNonEmptyAcc = i;
            break;
        }
    }

    for (int i = lastNonEmptyAcc + 1; i <= board.nnueData.size; i++)
        updateInputLayer(board, i, i == 0);

    auto& acc = board.nnueData.accumulator[board.nnueData.size];
    return head(acc.data[board.sideToMove], acc.data[~board.sideToMove]);
}

int NNUE::evaluate(Board& board) {
    return evaluateNet(board);
}

float NNUE::halfMoveScale(Board& board) {
    return (100.0f - board.halfMove) / 100.0f;
}

float NNUE::materialScale(Board& board) {
    float gamePhase = popcount64(board.bitboards[WHITE_KNIGHT] | board.bitboards[BLACK_KNIGHT] | board.bitboards[WHITE_BISHOP] | board.bitboards[BLACK_BISHOP]) * 3.0f;
    gamePhase += popcount64(board.bitboards[WHITE_ROOK] | board.bitboards[BLACK_ROOK]) * 5.0f;
    gamePhase += popcount64(board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN]) * 10.0f;

    gamePhase = std::min(gamePhase, 64.0f);

    constexpr float a = 0.8f;
    constexpr float b = 1.0f;

    return a + (b - a) * gamePhase / 64.0f;
}

bool NNUE::loadNetFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel) {
    auto fail = [&](const std::string& message) {
        std::cout << "info string Failed to load net from " << sourceLabel << ": " << message << std::endl;
        loaded = false;
        return false;
    };

    if (!data || size == 0)
        return fail("empty buffer");

    // Bullet quantised.bin: raw i16 arrays, no header
    // feature_weights: 768*1536*2 = 2359296 bytes
    // feature_bias:    1536*2     = 3072 bytes
    // output_weights:  3072*2     = 6144 bytes
    // output_bias:     1*2        = 2 bytes
    // Total: 2368514 bytes (may be padded to next 64-byte boundary)
    constexpr size_t exactSize = NNUE_FT_IN * NNUE_FT_OUT * 2
                               + NNUE_FT_OUT * 2
                               + 2 * NNUE_FT_OUT * 2
                               + 2;
    static_assert(exactSize == 2368514);

    if (size < exactSize)
        return fail("invalid file size (expected >= " + std::to_string(exactSize) + ", got " + std::to_string(size) + ")");

    const auto* src = reinterpret_cast<const int16_t*>(data);

    // feature_weights: 768*1536 i16 values (column-major 1536x768)
    std::memcpy(ftWeights, src, sizeof(ftWeights));
    src += NNUE_FT_IN * NNUE_FT_OUT;

    // feature_bias: 1536 i16 values
    std::memcpy(ftBiases, src, sizeof(ftBiases));
    src += NNUE_FT_OUT;

    // output_weights: 3072 i16 values ([us | them])
    std::memcpy(headWeights, src, sizeof(headWeights));
    src += 2 * NNUE_FT_OUT;

    // output_bias: 1 i16 value
    headBias = *src;

    std::cout << "info string Loaded Bullet net from " << sourceLabel
              << " (" << NNUE_FT_IN << " -> " << NNUE_FT_OUT << "x2 -> 1, SCReLU)" << std::endl;
    loaded = true;
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

    return loadNetFromBuffer(bytes.data(), bytes.size(), filePath);
}

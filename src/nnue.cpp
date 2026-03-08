#include "nnue.h"
#include "simd.h"
#include "incbin/incbin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#ifndef NET
    #define NET "devre_epoch300.bin"
#endif

INCBIN(EmbeddedNet, NET);

namespace {
constexpr char     NET_MAGIC[8]     = {'D', 'e', 'v', 'r', 'e', 'N', 'e', 't'};
constexpr float    NET_CP_SCALE     = 450.0f;
constexpr int      max_delta_rows   = 320;
constexpr uint32_t l1_cols          = 128;
constexpr uint32_t l2_rows          = 32;
constexpr uint32_t accum_simd_width = SIMD::vecSize;
static_assert(64 % accum_simd_width == 0);
static_assert(l1_cols % 64 == 0);
static_assert(l2_rows % accum_simd_width == 0);

inline __attribute__((always_inline)) void applyDeltaAddInt8(int16_t* __restrict__ accumulator, const int8_t* __restrict__ row) {
    for (uint32_t k = 0; k < l1_cols; k += accum_simd_width)
    {
        const SIMD::vecType delta = SIMD::vecLoadI8ToI16(row + k);
        const SIMD::vecType acc   = SIMD::vecLoad(accumulator + k);
        SIMD::vecStore(accumulator + k, SIMD::vecAddEpi16(acc, delta));
    }
}

inline __attribute__((always_inline)) void
applyDeltaBatchInt8(int16_t* __restrict__ accumulator, const int8_t* const* __restrict__ addRows, int addCount, const int8_t* const* __restrict__ subRows, int subCount) {
    for (uint32_t k = 0; k < l1_cols; k += accum_simd_width)
    {
        SIMD::vecType acc = SIMD::vecLoad(accumulator + k);

        for (int a = 0; a < addCount; a++)
            acc = SIMD::vecAddEpi16(acc, SIMD::vecLoadI8ToI16(addRows[a] + k));

        for (int s = 0; s < subCount; s++)
            acc = SIMD::vecSubEpi16(acc, SIMD::vecLoadI8ToI16(subRows[s] + k));

        SIMD::vecStore(accumulator + k, acc);
    }
}

}

NNUE* NNUE::Instance() {
    static NNUE instance = NNUE();
    return &instance;
}

NNUE::NNUE() {
    if (!loadNetFromBuffer(gEmbeddedNetData, gEmbeddedNetSize, "<embedded>"))
        std::cout << "info string Failed to load embedded net" << std::endl;
}

int NNUE::baseIndex(int piece, int sq, Color perspective) {
    const int orientedSquare = (perspective == BLACK) ? mirrorVertically(sq) : sq;
    const int pIdx           = pieceType(piece) + 6 * (pieceColor(piece) != perspective);
    return orientedSquare + pIdx * 64;
}

uint32_t NNUE::featureIndex(int i, int j) const {
    const uint32_t low  = static_cast<uint32_t>(std::min(i, j));
    const uint32_t high = static_cast<uint32_t>(std::max(i, j));
    return low * NNUE_BASE_FEATURES - (low * (low + 1)) / 2 + high;
}

const int8_t* NNUE::getPairWeights(uint32_t feature) const { return &l1Weights[static_cast<size_t>(feature) * l1_cols]; }

void NNUE::recalculateInputLayer(Board& board, Color perspective, int idx) {
    auto& acc        = board.nnueData.accumulator[idx];
    auto& active     = acc.baseActive[perspective];
    auto* activeList = acc.activeList[perspective];
    int   baseFeatures[N_SQUARES];
    int   activeCount = 0;

    active.reset();

    for (int sq = 0; sq < N_SQUARES; sq++)
    {
        const int piece = board.pieceBoard[sq];
        if (piece == EMPTY)
            continue;

        const int baseFeature = baseIndex(piece, sq, perspective);
        active.set(baseFeature);
        activeList[activeCount]     = static_cast<uint16_t>(baseFeature);
        baseFeatures[activeCount++] = baseFeature;
    }

    acc.activeCount[perspective] = static_cast<uint8_t>(activeCount);

    int16_t* out = acc.data[perspective];
    std::memcpy(out, l1Biases, static_cast<size_t>(l1_cols) * sizeof(int16_t));

    for (int a = 0; a < activeCount; a++)
    {
        for (int b = a; b < activeCount; b++)
        {
            const uint32_t feature = featureIndex(baseFeatures[a], baseFeatures[b]);
            applyDeltaAddInt8(out, getPairWeights(feature));
        }
    }
}

void NNUE::incrementalUpdateInputLayer(Board& board, Color perspective, int idx) {
    auto& prevAcc = board.nnueData.accumulator[idx - 1];
    auto& curAcc  = board.nnueData.accumulator[idx];

    auto& prevActive = prevAcc.baseActive[perspective];
    auto& curActive  = curAcc.baseActive[perspective];
    curActive        = prevActive;

    const int       prevCount = prevAcc.activeCount[perspective];
    int             curCount  = prevCount;
    const uint16_t* prevList  = prevAcc.activeList[perspective];
    uint16_t*       curList   = curAcc.activeList[perspective];
    std::memcpy(curList, prevList, static_cast<size_t>(prevCount) * sizeof(uint16_t));

    int16_t* out = curAcc.data[perspective];
    std::memcpy(out, prevAcc.data[perspective], static_cast<size_t>(l1_cols) * sizeof(int16_t));

    int removedBases[4];
    int addedBases[4];
    int removedBaseCount = 0;
    int addedBaseCount   = 0;

    const auto& changeAcc = board.nnueData.accumulator[idx];
    for (int i = 0; i < changeAcc.changeCount; i++)
    {
        const auto& change      = changeAcc.changes[i];
        const int   baseFeature = baseIndex(change.piece, change.sq, perspective);

        if (change.sign > 0)
        {
            if (!curActive.test(baseFeature))
            {
                curActive.set(baseFeature);
                addedBases[addedBaseCount++] = baseFeature;
                curList[curCount++]          = static_cast<uint16_t>(baseFeature);
            }
        }
        else if (curActive.test(baseFeature))
        {
            curActive.reset(baseFeature);
            removedBases[removedBaseCount++] = baseFeature;

            for (int j = 0; j < curCount; j++)
            {
                if (curList[j] == baseFeature)
                {
                    curList[j] = curList[curCount - 1];
                    curCount--;
                    break;
                }
            }
        }
    }

    curAcc.activeCount[perspective] = static_cast<uint8_t>(curCount);

    if (removedBaseCount == 0 && addedBaseCount == 0)
        return;

    int survivors[N_SQUARES];
    int survivorCount = 0;
    for (int i = 0; i < prevCount; i++)
    {
        const int prevBase = prevList[i];
        if (curActive.test(prevBase))
            survivors[survivorCount++] = prevBase;
    }

    const int8_t* addRows[max_delta_rows];
    const int8_t* subRows[max_delta_rows];

    int subRowCount = 0;
    for (int i = 0; i < removedBaseCount; i++)
    {
        subRows[subRowCount++] = getPairWeights(featureIndex(removedBases[i], removedBases[i]));
        for (int j = 0; j < survivorCount; j++)
            subRows[subRowCount++] = getPairWeights(featureIndex(removedBases[i], survivors[j]));
    }

    for (int i = 0; i < removedBaseCount; i++)
    {
        for (int j = i + 1; j < removedBaseCount; j++)
            subRows[subRowCount++] = getPairWeights(featureIndex(removedBases[i], removedBases[j]));
    }

    int addRowCount = 0;
    for (int i = 0; i < addedBaseCount; i++)
    {
        addRows[addRowCount++] = getPairWeights(featureIndex(addedBases[i], addedBases[i]));
        for (int j = 0; j < survivorCount; j++)
            addRows[addRowCount++] = getPairWeights(featureIndex(addedBases[i], survivors[j]));
    }

    for (int i = 0; i < addedBaseCount; i++)
    {
        for (int j = i + 1; j < addedBaseCount; j++)
            addRows[addRowCount++] = getPairWeights(featureIndex(addedBases[i], addedBases[j]));
    }

    applyDeltaBatchInt8(out, addRows, addRowCount, subRows, subRowCount);
}

void NNUE::updateInputLayer(Board& board, int idx, bool fromScratch) {
    if (idx == 0 || fromScratch || !board.nnueData.accumulator[idx - 1].nonEmpty)
    {
        recalculateInputLayer(board, WHITE, idx);
        recalculateInputLayer(board, BLACK, idx);
    }
    else
    {
        incrementalUpdateInputLayer(board, WHITE, idx);
        incrementalUpdateInputLayer(board, BLACK, idx);
    }
    board.nnueData.accumulator[idx].nonEmpty = true;
}

void NNUE::calculateInputLayer(Board& board, int idx, bool fromScratch) { updateInputLayer(board, idx, fromScratch); }

int NNUE::head(const int16_t* us, const int16_t* them) const {
    const int           clip          = l1Clip;
    const SIMD::vecType zero          = SIMD::vecZero();
    const SIMD::vecType clipVec       = SIMD::vecSet1Epi16(static_cast<int16_t>(clip));
    constexpr uint32_t  blocksPerSide = l1_cols / accum_simd_width;

    float logit         = l3Bias;
    auto  accumulateRow = [this, &logit](uint32_t row, SIMD::vecType acc) {
        const int32_t raw = l2Biases[row] + SIMD::vecReduceEpi32(acc);
        if (raw <= 0)
            return;

        const uint32_t clipped      = std::min<uint32_t>(static_cast<uint32_t>(raw), l2ClipByRow[row]);
        const float    clippedFloat = static_cast<float>(clipped);
        logit += l3Weights[row] * clippedFloat * clippedFloat;
    };
    constexpr uint32_t rowsPerTile = 8;
    static_assert(l2_rows % rowsPerTile == 0);

    for (uint32_t row = 0; row < l2_rows; row += rowsPerTile)
    {
        const int16_t* const __restrict__ weights0 = l2Weights.data() + static_cast<size_t>(row + 0) * 2 * l1_cols;
        const int16_t* const __restrict__ weights1 = l2Weights.data() + static_cast<size_t>(row + 1) * 2 * l1_cols;
        const int16_t* const __restrict__ weights2 = l2Weights.data() + static_cast<size_t>(row + 2) * 2 * l1_cols;
        const int16_t* const __restrict__ weights3 = l2Weights.data() + static_cast<size_t>(row + 3) * 2 * l1_cols;
        const int16_t* const __restrict__ weights4 = l2Weights.data() + static_cast<size_t>(row + 4) * 2 * l1_cols;
        const int16_t* const __restrict__ weights5 = l2Weights.data() + static_cast<size_t>(row + 5) * 2 * l1_cols;
        const int16_t* const __restrict__ weights6 = l2Weights.data() + static_cast<size_t>(row + 6) * 2 * l1_cols;
        const int16_t* const __restrict__ weights7 = l2Weights.data() + static_cast<size_t>(row + 7) * 2 * l1_cols;

        SIMD::vecType acc0 = SIMD::vecZero();
        SIMD::vecType acc1 = SIMD::vecZero();
        SIMD::vecType acc2 = SIMD::vecZero();
        SIMD::vecType acc3 = SIMD::vecZero();
        SIMD::vecType acc4 = SIMD::vecZero();
        SIMD::vecType acc5 = SIMD::vecZero();
        SIMD::vecType acc6 = SIMD::vecZero();
        SIMD::vecType acc7 = SIMD::vecZero();

        for (uint32_t block = 0; block < blocksPerSide; block++)
        {
            const uint32_t      offset      = block * accum_simd_width;
            const SIMD::vecType usRaw       = SIMD::vecLoad(us + offset);
            const SIMD::vecType usClipped   = SIMD::vecMinEpi16(clipVec, SIMD::vecMaxEpi16(zero, usRaw));
            const SIMD::vecType usAct       = SIMD::vecMulloEpi16(usClipped, usClipped);
            const SIMD::vecType themRaw     = SIMD::vecLoad(them + offset);
            const SIMD::vecType themClipped = SIMD::vecMinEpi16(clipVec, SIMD::vecMaxEpi16(zero, themRaw));
            const SIMD::vecType themAct     = SIMD::vecMulloEpi16(themClipped, themClipped);

            acc0 = SIMD::vecAddEpi32(acc0, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights0 + offset)));
            acc0 = SIMD::vecAddEpi32(acc0, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights0 + l1_cols + offset)));
            acc1 = SIMD::vecAddEpi32(acc1, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights1 + offset)));
            acc1 = SIMD::vecAddEpi32(acc1, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights1 + l1_cols + offset)));
            acc2 = SIMD::vecAddEpi32(acc2, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights2 + offset)));
            acc2 = SIMD::vecAddEpi32(acc2, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights2 + l1_cols + offset)));
            acc3 = SIMD::vecAddEpi32(acc3, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights3 + offset)));
            acc3 = SIMD::vecAddEpi32(acc3, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights3 + l1_cols + offset)));
            acc4 = SIMD::vecAddEpi32(acc4, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights4 + offset)));
            acc4 = SIMD::vecAddEpi32(acc4, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights4 + l1_cols + offset)));
            acc5 = SIMD::vecAddEpi32(acc5, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights5 + offset)));
            acc5 = SIMD::vecAddEpi32(acc5, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights5 + l1_cols + offset)));
            acc6 = SIMD::vecAddEpi32(acc6, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights6 + offset)));
            acc6 = SIMD::vecAddEpi32(acc6, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights6 + l1_cols + offset)));
            acc7 = SIMD::vecAddEpi32(acc7, SIMD::vecMaddEpi16(usAct, SIMD::vecLoad(weights7 + offset)));
            acc7 = SIMD::vecAddEpi32(acc7, SIMD::vecMaddEpi16(themAct, SIMD::vecLoad(weights7 + l1_cols + offset)));
        }

        accumulateRow(row + 0, acc0);
        accumulateRow(row + 1, acc1);
        accumulateRow(row + 2, acc2);
        accumulateRow(row + 3, acc3);
        accumulateRow(row + 4, acc4);
        accumulateRow(row + 5, acc5);
        accumulateRow(row + 6, acc6);
        accumulateRow(row + 7, acc7);
    }

    float cp = logit * NET_CP_SCALE;
    cp       = std::max(-1.0f * MAX_MATE_SCORE, std::min(cp, 1.0f * MAX_MATE_SCORE));
    return static_cast<int>(std::lround(cp));
}

int NNUE::evaluateNet(Board& board) {
    if (l1Weights.empty())
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

int NNUE::evaluate(Board& board) { return evaluateNet(board); }

float NNUE::halfMoveScale(Board& board) { return (100.0f - board.halfMove) / 100.0f; }

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
        return false;
    };

    if (!data || size == 0)
        return fail("empty buffer");

    const uint8_t*       cursor    = data;
    const uint8_t* const end       = data + size;
    auto                 readBytes = [&](void* dst, size_t bytes, const char* fieldName) {
        if (bytes > static_cast<size_t>(end - cursor))
            return fail(std::string("truncated at ") + fieldName);

        std::memcpy(dst, cursor, bytes);
        cursor += bytes;
        return true;
    };
    auto readValue = [&](auto& value, const char* fieldName) { return readBytes(&value, sizeof(value), fieldName); };

    char magic[sizeof(NET_MAGIC)]{};
    if (!readValue(magic, "magic"))
        return false;
    if (std::memcmp(magic, NET_MAGIC, sizeof(NET_MAGIC)) != 0)
        return fail("invalid magic");

    uint32_t version   = 0;
    float    l1Scale   = 0.0f;
    uint32_t l1Rows    = 0;
    uint32_t l1Cols    = 0;
    uint32_t l1BiasLen = 0;
    uint32_t l2Rows    = 0;
    uint32_t l2Cols    = 0;
    uint32_t l2BiasLen = 0;
    uint32_t l3Rows    = 0;
    uint32_t l3Cols    = 0;
    uint32_t l3BiasLen = 0;

    if (!readValue(version, "version") || !readValue(l1Scale, "l1_scale"))
        return false;

    if (!readValue(l1Rows, "l1_rows") || !readValue(l1Cols, "l1_cols") || !readValue(l1BiasLen, "l1_b_len") || !readValue(l2Rows, "l2_rows") || !readValue(l2Cols, "l2_cols")
        || !readValue(l2BiasLen, "l2_b_len") || !readValue(l3Rows, "l3_rows") || !readValue(l3Cols, "l3_cols") || !readValue(l3BiasLen, "l3_b_len"))
        return false;

    const bool dimsOk = l1Scale > 0.0f && l1Rows == NNUE_FEATURES && l1Cols == l1_cols && l1BiasLen == l1_cols && l2Rows == l2_rows && l2Cols == 2 * l1_cols && l2BiasLen == l2_rows && l3Rows == 1
                     && l3Cols == l2_rows && l3BiasLen == 1;
    if (!dimsOk)
        return fail("unexpected dimensions");

    const int newL1Clip = std::max(1, static_cast<int>(std::lround(l1Scale)));

    const size_t l1WeightCount = static_cast<size_t>(l1Rows) * l1Cols;
    const size_t l1BiasCount   = l1BiasLen;
    const size_t l2WeightCount = static_cast<size_t>(l2Rows) * l2Cols;
    const size_t l2BiasCount   = l2BiasLen;
    const size_t l3WeightCount = static_cast<size_t>(l3Rows) * l3Cols;

    l1Clip = newL1Clip;
    l1Weights.resize(l1WeightCount);
    l2Weights.resize(l2WeightCount);

    if (!readBytes(l2ClipByRow, l2Rows * sizeof(uint32_t), "l2_row_scales") || !readBytes(l1Weights.data(), l1Weights.size() * sizeof(int8_t), "l1_w"))
    {
        return false;
    }

    for (uint32_t i = 0; i < l2Rows; i++)
    {
        if (l2ClipByRow[i] == 0)
            return fail("l2_row_scales must be positive");
    }

    for (uint32_t i = 0; i < l1BiasCount; i++)
    {
        int8_t bias = 0;
        if (!readValue(bias, "l1_b"))
            return false;

        l1Biases[i] = bias;
    }

    if (!readBytes(l2Weights.data(), l2Weights.size() * sizeof(int16_t), "l2_w") || !readBytes(l2Biases, l2BiasCount * sizeof(int32_t), "l2_b")
        || !readBytes(l3Weights, l3WeightCount * sizeof(float), "l3_w") || !readValue(l3Bias, "l3_b"))
    {
        return false;
    }

    for (uint32_t i = 0; i < l3WeightCount; i++)
    {
        const float scale = static_cast<float>(l2ClipByRow[i]);
        l3Weights[i] /= scale * scale;
    }

    std::cout << "info string Loaded DevreNet v" << version << " from " << sourceLabel << " (" << l1Rows << "x" << l1Cols << " -> " << l2Rows << " -> 1)" << std::endl;
    return true;
}

bool NNUE::loadNet(const std::string& filePath) {
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

bool NNUE::loadNetwork(const std::string& filePath) { return loadNet(filePath); }

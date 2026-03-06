#include "nnue.h"
#include "simd.h"
#include "incbin/incbin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <type_traits>
#include <utility>

#ifndef NET
    #define NET "devre_epoch300.bin"
#endif

INCBIN(EmbeddedRelationNet, NET);

namespace {
constexpr char RELATION_NET_MAGIC[8] = {'D', 'e', 'v', 'r', 'e', 'N', 'e', 't'};
constexpr uint32_t RELATION_NET_VERSION = 2;
constexpr float RELATION_NET_CP_SCALE = 375.0f;
constexpr int max_rel_deltas = 320;
constexpr uint32_t l1_cols = 128;
constexpr uint32_t l2_rows = 32;
constexpr uint32_t accum_simd_width = static_cast<uint32_t>(SIMD::vecSize);
constexpr size_t l2_blocks = l2_rows / SIMD::f32Size;
static_assert(64 % accum_simd_width == 0);
static_assert(l1_cols % 64 == 0);
static_assert(l2_rows % SIMD::f32Size == 0);

template<size_t... I, typename Fn>
inline __attribute__((always_inline)) void constexprFor(std::index_sequence<I...>, Fn&& fn) {
    (fn(std::integral_constant<size_t, I>{}), ...);
}

inline __attribute__((always_inline)) void applyRelationDeltaAddImpl(int16_t* __restrict__ accumulator,
                                                                     const int8_t* __restrict__ row,
                                                                     uint32_t cols) {
    for (uint32_t k = 0; k < cols; k += accum_simd_width)
    {
        const SIMD::vecType delta = SIMD::vecLoadI8ToI16(row + k);
        const SIMD::vecType acc = SIMD::vecLoad(accumulator + k);
        SIMD::vecStore(accumulator + k, SIMD::vecAddEpi16(acc, delta));
    }
}

inline __attribute__((always_inline)) void applyRelationDeltaBatchImpl(int16_t* __restrict__ accumulator,
                                                                       const int8_t* const* __restrict__ addRows,
                                                                       int addCount,
                                                                       const int8_t* const* __restrict__ subRows,
                                                                       int subCount,
                                                                       uint32_t cols) {
    for (uint32_t k = 0; k < cols; k += accum_simd_width)
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
    for (int i = 0; i < RELATION_BASE_FEATURES; i++)
    {
        const int base = i * RELATION_BASE_FEATURES;
        const int rowStart = i * RELATION_BASE_FEATURES - (i * (i + 1)) / 2;
        for (int j = i; j < RELATION_BASE_FEATURES; j++)
        {
            const uint32_t rel = static_cast<uint32_t>(rowStart + j);
            relationLut[base + j] = rel;
            relationLut[j * RELATION_BASE_FEATURES + i] = rel;
        }
    }

    if (!loadRelationNetworkFromBuffer(gEmbeddedRelationNetData, gEmbeddedRelationNetSize, "<embedded>"))
        std::cout << "info string Failed to load embedded relation net" << std::endl;
}

int NNUE::relationBaseIndex(int piece, int sq, Color perspective) {
    const int orientedSquare = (perspective == BLACK) ? mirrorVertically(sq) : sq;
    const int pIdx = pieceType(piece) + 6 * (pieceColor(piece) != perspective);
    return orientedSquare + pIdx * 64;
}

uint32_t NNUE::relationFeatureIndex(int i, int j) const {
    return relationLut[i * RELATION_BASE_FEATURES + j];
}

const int8_t* NNUE::relationRow(uint32_t relationIndex) const {
    return &relationL1Weights[static_cast<size_t>(relationIndex) * relationL1Cols];
}

void NNUE::applyRelationDeltaAdd(int16_t* accumulator, uint32_t relationIndex) const {
    applyRelationDeltaAddImpl(accumulator, relationRow(relationIndex), relationL1Cols);
}

void NNUE::applyRelationDeltaBatch(int16_t* accumulator, const int8_t* const* addRows, int addCount, const int8_t* const* subRows, int subCount) const {
    applyRelationDeltaBatchImpl(accumulator, addRows, addCount, subRows, subCount, relationL1Cols);
}

void NNUE::recalculateInputLayer(Board& board, Color perspective, int idx) {
    auto& relationAcc = board.nnueData.relationAccumulator[idx];
    auto& active = relationAcc.baseActive[perspective];
    auto* activeList = relationAcc.activeList[perspective];
    int activeCount = 0;
    active.reset();

    std::array<int, N_SQUARES> baseIndices{};
    int baseCount = 0;
    for (int sq = 0; sq < N_SQUARES; sq++)
    {
        const int piece = board.pieceBoard[sq];
        if (piece == EMPTY)
            continue;

        const int baseIndex = relationBaseIndex(piece, sq, perspective);
        active.set(baseIndex);
        activeList[activeCount++] = static_cast<uint16_t>(baseIndex);
        baseIndices[baseCount++] = baseIndex;
    }
    relationAcc.activeCount[perspective] = static_cast<uint8_t>(activeCount);

    int16_t* out = relationAcc.data[perspective];
    std::memcpy(out, relationL1Biases, static_cast<size_t>(relationL1Cols) * sizeof(int16_t));

    for (int a = 0; a < baseCount; a++)
    {
        for (int b = a; b < baseCount; b++)
            applyRelationDeltaAdd(out, relationFeatureIndex(baseIndices[a], baseIndices[b]));
    }
}

void NNUE::incrementalUpdateInputLayer(Board& board, Color perspective, int idx) {
    auto& prevAcc = board.nnueData.relationAccumulator[idx - 1];
    auto& curAcc = board.nnueData.relationAccumulator[idx];

    auto& prevActive = prevAcc.baseActive[perspective];
    auto& curActive = curAcc.baseActive[perspective];
    curActive = prevActive;

    const int prevCount = prevAcc.activeCount[perspective];
    int curCount = prevCount;
    const uint16_t* prevList = prevAcc.activeList[perspective];
    uint16_t* curList = curAcc.activeList[perspective];
    std::memcpy(curList, prevList, static_cast<size_t>(prevCount) * sizeof(uint16_t));

    int16_t* out = curAcc.data[perspective];
    const int16_t* prevOut = prevAcc.data[perspective];
    std::memcpy(out, prevOut, static_cast<size_t>(relationL1Cols) * sizeof(int16_t));

    int removed[4];
    int added[4];
    int removedCount = 0;
    int addedCount = 0;

    auto removeFromCurList = [&](int baseIndex) {
        for (int i = 0; i < curCount; i++)
        {
            if (curList[i] == baseIndex)
            {
                curList[i] = curList[curCount - 1];
                curCount--;
                return;
            }
        }
    };

    const auto& changeAcc = board.nnueData.relationAccumulator[idx];
    for (int i = 0; i < changeAcc.changeCount; i++)
    {
        const auto& change = changeAcc.changes[i];
        const int baseIndex = relationBaseIndex(change.piece, change.sq, perspective);

        if (change.sign > 0)
        {
            if (!curActive.test(baseIndex))
            {
                curActive.set(baseIndex);
                added[addedCount++] = baseIndex;
                curList[curCount++] = static_cast<uint16_t>(baseIndex);
            }
        }
        else
        {
            if (curActive.test(baseIndex))
            {
                curActive.reset(baseIndex);
                removed[removedCount++] = baseIndex;
                removeFromCurList(baseIndex);
            }
        }
    }
    curAcc.activeCount[perspective] = static_cast<uint8_t>(curCount);

    if (removedCount == 0 && addedCount == 0)
        return;

    int survivors[N_SQUARES];
    int survivorCount = 0;
    for (int i = 0; i < prevCount; i++)
    {
        const int idxBase = prevList[i];
        bool wasRemoved = false;
        for (int r = 0; r < removedCount; r++)
        {
            if (removed[r] == idxBase)
            {
                wasRemoved = true;
                break;
            }
        }

        if (!wasRemoved)
            survivors[survivorCount++] = idxBase;
    }

    const int8_t* addRows[max_rel_deltas];
    const int8_t* subRows[max_rel_deltas];

    if (removedCount == 1 && addedCount == 1)
    {
        const int removedBase = removed[0];
        const int addedBase = added[0];
        int addRowCount = 0;
        int subRowCount = 0;

        addRows[addRowCount++] = relationRow(relationFeatureIndex(addedBase, addedBase));
        subRows[subRowCount++] = relationRow(relationFeatureIndex(removedBase, removedBase));
        for (int u = 0; u < survivorCount; u++)
        {
            addRows[addRowCount++] = relationRow(relationFeatureIndex(addedBase, survivors[u]));
            subRows[subRowCount++] = relationRow(relationFeatureIndex(removedBase, survivors[u]));
        }
        applyRelationDeltaBatch(out, addRows, addRowCount, subRows, subRowCount);
        return;
    }

    int subRowCount = 0;
    for (int i = 0; i < removedCount; i++)
    {
        const int r = removed[i];
        subRows[subRowCount++] = relationRow(relationFeatureIndex(r, r));
        for (int u = 0; u < survivorCount; u++)
            subRows[subRowCount++] = relationRow(relationFeatureIndex(r, survivors[u]));
    }

    for (int i = 0; i < removedCount; i++)
    {
        for (int j = i + 1; j < removedCount; j++)
            subRows[subRowCount++] = relationRow(relationFeatureIndex(removed[i], removed[j]));
    }

    int addRowCount = 0;
    for (int i = 0; i < addedCount; i++)
    {
        const int a = added[i];
        addRows[addRowCount++] = relationRow(relationFeatureIndex(a, a));
        for (int u = 0; u < survivorCount; u++)
            addRows[addRowCount++] = relationRow(relationFeatureIndex(a, survivors[u]));
    }

    for (int i = 0; i < addedCount; i++)
    {
        for (int j = i + 1; j < addedCount; j++)
            addRows[addRowCount++] = relationRow(relationFeatureIndex(added[i], added[j]));
    }
    applyRelationDeltaBatch(out, addRows, addRowCount, subRows, subRowCount);
}

void NNUE::updateInputLayer(Board& board, int idx, bool fromScratch) {
    if (idx == 0 || fromScratch || !board.nnueData.relationAccumulator[idx - 1].nonEmpty)
    {
        recalculateInputLayer(board, WHITE, idx);
        recalculateInputLayer(board, BLACK, idx);
    }
    else
    {
        incrementalUpdateInputLayer(board, WHITE, idx);
        incrementalUpdateInputLayer(board, BLACK, idx);
    }
    board.nnueData.relationAccumulator[idx].nonEmpty = true;
}

void NNUE::calculateInputLayer(Board& board, int idx, bool fromScratch) {
    updateInputLayer(board, idx, fromScratch);
}

int NNUE::relationHead(const int16_t* us, const int16_t* them) const {
    alignas(64) SIMD::f32Type hidden[l2_blocks];
    constexprFor(std::make_index_sequence<l2_blocks>{}, [&](auto blockIdx) {
        constexpr size_t block = decltype(blockIdx)::value;
        hidden[block] = SIMD::f32Load(relationL2Biases + block * SIMD::f32Size);
    });

    const float* const __restrict__ lut = relationL1ActivationLut.data();
    const int clip = relationL1Clip;

    auto accumulateInput = [&](int raw, const float* __restrict__ weights) {
        if (raw <= 0)
            return;

        const float activation = lut[raw < clip ? raw : clip];
        const SIMD::f32Type act = SIMD::f32Set1(activation);

        constexprFor(std::make_index_sequence<l2_blocks>{}, [&](auto blockIdx) {
            constexpr size_t block = decltype(blockIdx)::value;
            hidden[block] = SIMD::f32Fmadd(act, SIMD::f32Load(weights + block * SIMD::f32Size), hidden[block]);
        });
    };

    auto accumulateSide = [&](const int16_t* __restrict__ side, const float* __restrict__ weightsBase) {
        for (uint32_t i = 0; i < l1_cols; i += 4)
        {
            constexprFor(std::make_index_sequence<4>{}, [&](auto laneIdx) {
                constexpr size_t lane = decltype(laneIdx)::value;
                accumulateInput(side[i + lane], weightsBase + (i + lane) * l2_rows);
            });
        }
    };

    accumulateSide(us, relationL2WeightsByInput.data());
    accumulateSide(them, relationL2WeightsThem);

    const SIMD::f32Type zero = SIMD::f32SetZero();
    const SIMD::f32Type one = SIMD::f32Set1(1.0f);
    const SIMD::f32Type invScale = SIMD::f32Set1(relationInvL2Scale);
    SIMD::f32Type sum = SIMD::f32SetZero();

    constexprFor(std::make_index_sequence<l2_blocks>{}, [&](auto blockIdx) {
        constexpr size_t block = decltype(blockIdx)::value;
        SIMD::f32Type h = SIMD::f32Mul(hidden[block], invScale);
        h = SIMD::f32Min(one, SIMD::f32Max(zero, h));
        h = SIMD::f32Mul(h, h);
        sum = SIMD::f32Fmadd(h, SIMD::f32Load(relationL3Weights + block * SIMD::f32Size), sum);
    });

    const float logit = relationL3Bias + SIMD::f32ReduceAdd(sum);
    float cp = logit * RELATION_NET_CP_SCALE;
    cp = std::clamp(cp, -1.0f * MAX_MATE_SCORE, 1.0f * MAX_MATE_SCORE);
    return static_cast<int>(std::lround(cp));
}

int NNUE::evaluateRelationNet(Board& board) {
    if (relationL1Weights.empty() || relationL2WeightsByInput.empty())
        return 0;

    int lastNonEmptyAcc = -1;
    for (int i = board.nnueData.size; i >= 0; i--)
    {
        if (board.nnueData.relationAccumulator[i].nonEmpty)
        {
            lastNonEmptyAcc = i;
            break;
        }
    }

    for (int i = lastNonEmptyAcc + 1; i <= board.nnueData.size; i++)
        updateInputLayer(board, i, i == 0);

    auto& acc = board.nnueData.relationAccumulator[board.nnueData.size];
    return relationHead(acc.data[board.sideToMove], acc.data[~board.sideToMove]);
}

int NNUE::evaluate(Board& board) {
    return evaluateRelationNet(board);
}

float NNUE::halfMoveScale(Board& board) { return (100.0f - board.halfMove) / 100.0f; }

float NNUE::materialScale(Board& board) {
    float gamePhase = popcount64(board.bitboards[WHITE_KNIGHT] | board.bitboards[BLACK_KNIGHT] | board.bitboards[WHITE_BISHOP] | board.bitboards[BLACK_BISHOP]) * 3;
    gamePhase += popcount64(board.bitboards[WHITE_ROOK] | board.bitboards[BLACK_ROOK]) * 5;
    gamePhase += popcount64(board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN]) * 10;

    gamePhase = std::min(gamePhase, 64.0f);

    constexpr float a = 0.8f;
    constexpr float b = 1.0f;

    return a + (b - a) * gamePhase / 64.0f;
}

bool NNUE::loadRelationNetworkFromBuffer(const uint8_t* data, size_t size, const std::string& sourceLabel) {
    if (!data || size == 0)
    {
        std::cout << "info string Empty relation net buffer: " << sourceLabel << std::endl;
        return false;
    }

    const uint8_t* cursor = data;
    const uint8_t* const end = data + size;
    auto readBytes = [&](void* dst, size_t bytes, const char* fieldName) {
        if (bytes > static_cast<size_t>(end - cursor))
        {
            std::cout << "info string Failed to read " << fieldName << std::endl;
            return false;
        }
        std::memcpy(dst, cursor, bytes);
        cursor += bytes;
        return true;
    };
    auto readValue = [&](auto& value, const char* fieldName) {
        return readBytes(&value, sizeof(value), fieldName);
    };

    char magic[sizeof(RELATION_NET_MAGIC)]{};
    if (!readValue(magic, "magic"))
        return false;
    if (std::memcmp(magic, RELATION_NET_MAGIC, sizeof(RELATION_NET_MAGIC)) != 0)
    {
        std::cout << "info string Invalid relation net magic" << std::endl;
        return false;
    }

    uint32_t version = 0;
    float l1Scale = 0.0f;
    float l2Scale = 0.0f;
    float l3Scale = 0.0f;
    uint32_t l1Rows = 0;
    uint32_t l1Cols = 0;
    uint32_t l1BiasLen = 0;
    uint32_t l2Rows = 0;
    uint32_t l2Cols = 0;
    uint32_t l2BiasLen = 0;
    uint32_t l3Rows = 0;
    uint32_t l3Cols = 0;
    uint32_t l3BiasLen = 0;

    if (!readValue(version, "version")
        || !readValue(l1Scale, "l1_scale")
        || !readValue(l2Scale, "l2_scale")
        || !readValue(l3Scale, "l3_scale")
        || !readValue(l1Rows, "l1_rows")
        || !readValue(l1Cols, "l1_cols")
        || !readValue(l1BiasLen, "l1_b_len")
        || !readValue(l2Rows, "l2_rows")
        || !readValue(l2Cols, "l2_cols")
        || !readValue(l2BiasLen, "l2_b_len")
        || !readValue(l3Rows, "l3_rows")
        || !readValue(l3Cols, "l3_cols")
        || !readValue(l3BiasLen, "l3_b_len"))
    {
        return false;
    }

    if (version != RELATION_NET_VERSION
        || l1Rows != RELATION_FEATURES
        || l1Cols != l1_cols
        || l1BiasLen != l1_cols
        || l1Scale <= 0.0f
        || l2Scale <= 0.0f
        || l3Scale <= 0.0f
        || l2Rows != l2_rows
        || l2Cols != 2 * l1_cols
        || l2BiasLen != l2_rows
        || l3Rows != 1
        || l3Cols != l2_rows
        || l3BiasLen != 1)
    {
        std::cout << "info string Invalid relation net dimensions" << std::endl;
        return false;
    }

    const size_t l1WeightCount = static_cast<size_t>(l1Rows) * l1Cols;
    const size_t l1BiasCount = l1BiasLen;
    const size_t l2WeightCount = static_cast<size_t>(l2Rows) * l2Cols;
    const size_t l2BiasCount = l2BiasLen;
    const size_t l3WeightCount = static_cast<size_t>(l3Rows) * l3Cols;
    const size_t l3BiasCount = l3BiasLen;

    std::vector<int8_t, AlignedAllocator<int8_t, 64>> l1Weights(l1WeightCount);
    std::vector<int8_t> l1Biases(l1BiasCount);
    std::vector<int16_t> l2Weights(l2WeightCount);
    std::vector<int16_t> l2Biases(l2BiasCount);
    std::vector<int32_t> l3Weights(l3WeightCount);
    std::vector<int32_t> l3Biases(l3BiasCount);

    if (!readBytes(l1Weights.data(), l1Weights.size() * sizeof(int8_t), "l1_w")
        || !readBytes(l1Biases.data(), l1Biases.size() * sizeof(int8_t), "l1_b")
        || !readBytes(l2Weights.data(), l2Weights.size() * sizeof(int16_t), "l2_w")
        || !readBytes(l2Biases.data(), l2Biases.size() * sizeof(int16_t), "l2_b")
        || !readBytes(l3Weights.data(), l3Weights.size() * sizeof(int32_t), "l3_w")
        || !readBytes(l3Biases.data(), l3Biases.size() * sizeof(int32_t), "l3_b"))
    {
        return false;
    }

    std::vector<float, AlignedAllocator<float, 64>> l2WeightsByInput(static_cast<size_t>(l2Cols) * l2Rows);
    for (uint32_t row = 0; row < l2Rows; row++)
    {
        const size_t rowOffset = static_cast<size_t>(row) * l2Cols;
        for (uint32_t col = 0; col < l2Cols; col++)
            l2WeightsByInput[static_cast<size_t>(col) * l2Rows + row] = static_cast<float>(l2Weights[rowOffset + col]);
    }

    relationL1Scale = l1Scale;
    relationL2Scale = l2Scale;
    relationL3Scale = l3Scale;
    relationInvL1Squared = 1.0f / (relationL1Scale * relationL1Scale);
    relationInvL2Scale = 1.0f / relationL2Scale;
    relationL1Rows = l1Rows;
    relationL1Cols = l1Cols;
    relationL2Rows = l2Rows;
    relationL1Clip = std::max(1, static_cast<int>(std::lround(relationL1Scale)));

    relationL1Weights = std::move(l1Weights);
    relationL2WeightsByInput = std::move(l2WeightsByInput);
    relationL2WeightsThem = relationL2WeightsByInput.data() + static_cast<size_t>(relationL1Cols) * relationL2Rows;
    std::fill(relationL1Biases, relationL1Biases + RELATION_L1_MAX, 0);
    std::fill(relationL2Biases, relationL2Biases + RELATION_L2_MAX, 0.0f);
    std::fill(relationL3Weights, relationL3Weights + RELATION_L2_MAX, 0.0f);
    for (uint32_t i = 0; i < relationL1Cols; i++)
        relationL1Biases[i] = static_cast<int16_t>(l1Biases[i]);
    for (uint32_t i = 0; i < relationL2Rows; i++)
    {
        relationL2Biases[i] = static_cast<float>(l2Biases[i]);
        relationL3Weights[i] = static_cast<float>(l3Weights[i]) / relationL3Scale;
    }
    relationL3Bias = static_cast<float>(l3Biases[0]) / relationL3Scale;
    relationL1ActivationLut.resize(static_cast<size_t>(relationL1Clip) + 1);
    for (int i = 0; i <= relationL1Clip; i++)
    {
        const float x = std::min(static_cast<float>(i), relationL1Scale);
        relationL1ActivationLut[i] = x * x * relationInvL1Squared;
    }

    std::cout << "info string DevreNet mixed loaded from: " << sourceLabel << std::endl;
    std::cout << "info string l1_rows " << relationL1Rows
              << " l1_cols " << relationL1Cols
              << " l2_rows " << relationL2Rows
              << " l1_scale " << relationL1Scale
              << " l2_scale " << relationL2Scale
              << " l3_scale " << relationL3Scale << std::endl;
    return true;
}

bool NNUE::loadRelationNetwork(const std::string& filePath) {
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

    return loadRelationNetworkFromBuffer(bytes.data(), bytes.size(), filePath);
}

bool NNUE::loadNetwork(const std::string& filePath) {
    return loadRelationNetwork(filePath);
}

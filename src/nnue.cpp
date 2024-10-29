#include "nnue.h"
#include "simd.h"

using namespace SIMD;
constexpr int nnueIndexMapping[2][14] = {{0, 1, 2, 3, 4, 5, 0, 0, 6, 7, 8, 9, 10, 11}, {6, 7, 8, 9, 10, 11, 0, 0, 0, 1, 2, 3, 4, 5}};

#ifndef NET
    #define NET "devre_06.08.2024.nnue"
#endif
INCBIN(EmbeddedWeights, NET);

NNUE* NNUE::Instance() {
    static NNUE instance = NNUE();
    return &instance;
}

NNUE::NNUE() {
    int16_t* data_int16;
    data_int16 = (int16_t*) (gEmbeddedWeightsData);

    for (int16_t& feature_weight : feature_weights)
        feature_weight = (*data_int16++);

    for (int16_t& feature_bias : feature_biases)
        feature_bias = (*data_int16++);

    for (int16_t& layer1_weight : layer1_weights)
        layer1_weight = (*data_int16++);

    for (int16_t& bias : layer1_bias)
        bias = (*data_int16++);
}


int NNUE::nnueIndex(int king, int piece, int sq, int side) {
    if (side == BLACK)
        sq = mirrorVertically(sq);
    return nnueIndexMapping[side][piece] * 64 + sq;
}

int NNUE::calculateIndices(Board& board, int (&weightIndices)[N_SQUARES], Color c) {
    int king = bitScanForward(board.bitboards[pieceIndex(c, KING)]);
    int sz   = 0;

    for (int k = 0; k < 64; k++)
    {
        if (board.pieceBoard[k] != EMPTY)
        {
            weightIndices[sz++] = L1 * nnueIndex(king, board.pieceBoard[k], k, c);
        }
    }
    return sz;
}

void NNUE::incrementalUpdate(Board& board, Color c, int idx) {
    int king = bitScanForward(board.bitboards[pieceIndex(c, KING)]);

    vecType* weightAdd[2] = {nullptr};
    vecType* weightSub[2] = {nullptr};

    int j = 0, k = 0;
    for (int i = 0; i < board.nnueData.accumulator[idx].numberOfChange; i++)
    {
        const auto& element = board.nnueData.accumulator[idx].nnueChanges[i];
        if (element.sign == 1)
        {
            auto featureIndex = nnueIndex(king, element.piece, element.sq, c);
            weightAdd[j++]    = (vecType*) &feature_weights[L1 * featureIndex];
        }
        else
        {
            auto featureIndex = nnueIndex(king, element.piece, element.sq, c);
            weightSub[k++]    = (vecType*) &feature_weights[L1 * featureIndex];
        }
    }

    auto* acc     = (vecType*) &board.nnueData.accumulator[idx - 1].data[c];
    auto* outputs = (vecType*) &board.nnueData.accumulator[idx].data[c];

    for (int i = 0; i < L1 / vecSize; i++)
    {
        outputs[i] = vecSubEpi16(weightAdd[0][i], weightSub[0][i]);
        outputs[i] = vecAddEpi16(outputs[i], acc[i]);

        if (weightAdd[1] != nullptr)
            outputs[i] = vecAddEpi16(outputs[i], weightAdd[1][i]);

        if (weightSub[1] != nullptr)
            outputs[i] = vecSubEpi16(outputs[i], weightSub[1][i]);
    }
}

int32_t NNUE::quanMatrixMultp(int16_t* us, int16_t* them, const int16_t* weights, const int16_t bias) {
    auto zero = vecSetZeroEpi16();
    auto one  = vecSetEpi16(QA);

    auto* vepi16Us      = (vecType*) &us[0];
    auto* vepi16Them    = (vecType*) &them[0];
    auto* vepi16Weights = (vecType*) weights;
    auto  out           = vecSetZeroEpi16();

    int weightOffset = 0;
    for (const auto* acc : {vepi16Us, vepi16Them})
    {
        for (int i = 0; i < L1 / vecSize; i++)
        {
            vecType clipped = vecMinEpi16(one, vecMaxEpi16(zero, acc[i]));
            out             = vecAddEpi32(out, vecMaddEpi16(vecMulloEpi16(clipped, clipped), vepi16Weights[i]));
        }

        vepi16Weights = (vecType*) &weights[L1];
    }

    int32_t sum            = 0;
    sum                    = vecReduceEpi32(out);
    float constexpr scalar = NET_SCALE / (QA * QB);
    return (sum / QA + bias) * scalar;
}


void NNUE::recalculateInputLayer(Board& board, Color c, int idx) {
    int weightIndices[N_SQUARES];
    int sz = calculateIndices(board, weightIndices, c);

    auto* biases  = (vecType*) &feature_biases[0];
    auto* outputs = (vecType*) &board.nnueData.accumulator[idx].data[c][0];  //white

    auto* weights = (vecType*) &feature_weights[weightIndices[0]];
    for (int i = 0; i < L1 / vecSize; i++)
    {
        outputs[i] = vecAddEpi16(biases[i], weights[i]);
    }
    for (int k = 1; k < sz; k++)
    {
        weights = (vecType*) &feature_weights[weightIndices[k]];
        for (int i = 0; i < L1 / vecSize; i++)
        {
            outputs[i] = vecAddEpi16(outputs[i], weights[i]);
        }
    }
}

void NNUE::calculateInputLayer(Board& board, int idx, bool fromScratch) {
    if (idx == 0 || fromScratch)
    {
        recalculateInputLayer(board, WHITE, idx);
        recalculateInputLayer(board, BLACK, idx);
    }
    else
    {
        incrementalUpdate(board, WHITE, idx);
        incrementalUpdate(board, BLACK, idx);
    }
    board.nnueData.accumulator[idx].nonEmpty = true;
}

int NNUE::evaluate(Board& board) {

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
    {
        calculateInputLayer(board, i);
    }

    auto      us           = board.nnueData.accumulator[board.nnueData.size].data[board.sideToMove];
    auto      enemy        = board.nnueData.accumulator[board.nnueData.size].data[1 - board.sideToMove];
    const int outputBucket = 0;

    int eval = quanMatrixMultp(us, enemy, &layer1_weights[2 * L1 * outputBucket], layer1_bias[outputBucket]);
    return eval;
}

float NNUE::halfMoveScale(Board& board) { return (100.0f - board.halfMove) / 100.0f; }

float NNUE::materialScale(Board& board) {
    float gamePhase = popcount64(board.bitboards[WHITE_KNIGHT] | board.bitboards[BLACK_KNIGHT] | board.bitboards[WHITE_BISHOP] | board.bitboards[BLACK_BISHOP]) * 3;
    gamePhase += popcount64(board.bitboards[WHITE_ROOK] | board.bitboards[BLACK_ROOK]) * 5;
    gamePhase += popcount64(board.bitboards[WHITE_QUEEN] | board.bitboards[BLACK_QUEEN]) * 10;

    gamePhase = std::min(gamePhase, 64.0f);

    float a = 0.8;
    float b = 1.0;

    //[a,b]
    return a + (b - a) * gamePhase / 64.0f;
}

#include "nnue.h"
#include "move.h"

int kingIndex[64] = {
        0, 1, 2, 3, 3, 2, 1, 0,
        4, 5, 6, 7, 7, 6, 5, 4,
        8, 9, 10, 11, 11, 10, 9, 8,
        12, 13, 14, 15, 15, 14, 13, 12,
        16, 17, 18, 19, 19, 18, 17, 16,
        20, 21, 22, 23, 23, 22, 21, 20,
        24, 25, 26, 27, 27, 26, 25, 24,
        28, 29, 30, 31, 31, 30, 29, 28};

constexpr int nnueIndexMapping[2][14] = {{0, 1, 2, 3, 4,  5,  0, 0, 6, 7, 8, 9, 10, 11},
                                         {6, 7, 8, 9, 10, 11, 0, 0, 0, 1, 2, 3, 4,  5}};
INCBIN(EmbeddedWeights, "devre_23.09.22_ep120.nnue");
NNUE *NNUE::Instance() {
    static NNUE instance = NNUE();
    return &instance;
}

NNUE::NNUE() {

    int16_t *data_int16;
    data_int16 = (int16_t *) (gEmbeddedWeightsData);

    for (int16_t &feature_weight: feature_weights)
        feature_weight = (*data_int16++);

    for (int16_t &feature_bias: feature_biases)
        feature_bias = (*data_int16++);

    for (int16_t &layer1_weight: layer1_weights)
        layer1_weight = (*data_int16++);

    layer1_bias = *(int32_t *) (data_int16);
}


int NNUE::nnueIndex(int king, int piece, int sq, int side) {
    if (side == BLACK) {
        sq = mirrorVertically(sq);
        king = mirrorVertically(king);
    }
    if (king % 8 < 4)
        return kingIndex[king] * 768 + nnueIndexMapping[side][piece] * 64 + mirrorHorizontally(sq);
    else
        return kingIndex[king] * 768 + nnueIndexMapping[side][piece] * 64 + sq;

}

int NNUE::calculateIndices(Board &board, int (&weightIndices)[N_SQUARES], Color c) {
    int king = bitScanForward(board.bitboards[pieceIndex(c, KING)]);
    int sz = 0;

    for (int k = 0; k < 64; k++) {
        if (board.pieceBoard[k] != EMPTY) {
            weightIndices[sz++] = L1 * nnueIndex(king, board.pieceBoard[k], k, c);
        }
    }
    return sz;
}

void NNUE::incrementalUpdate(Board &board, Color c) {
    int king = bitScanForward(board.bitboards[pieceIndex(c, KING)]);

    int pieceAddition[2];
    int pieceRemoval[2];
    int j = 0, k = 0;

    for (auto &element: board.nnueData.nnueChanges) {
        if (element.sign == 1) {
            pieceAddition[j++] = nnueIndex(king, element.piece, element.sq, c);
        } else {
            pieceRemoval[k++] = nnueIndex(king, element.piece, element.sq, c);
        }
    }

    auto *acc = (vector_type *) &board.nnueData.accumulator[board.nnueData.size - 1][c];
    auto *outputs = (vector_type *) &board.nnueData.accumulator[board.nnueData.size][c];

    auto *weights = (vector_type *) &feature_weights[L1 * pieceAddition[--j]];

    for (int i = 0; i < L1 / vector_size; i++) {
        outputs[i] = vector_add(acc[i], weights[i]);
    }

    while (j--) {
        weights = (vector_type *) &feature_weights[L1 * pieceAddition[j]];
        for (int i = 0; i < L1 / vector_size; i++) {
            outputs[i] = vector_add(outputs[i], weights[i]);
        }
    }

    while (k--) {
        weights = (vector_type *) &feature_weights[L1 * pieceRemoval[k]];
        for (int i = 0; i < L1 / vector_size; i++) {
            outputs[i] = vector_sub(outputs[i], weights[i]);
        }
    }
}


int32_t NNUE::quanMatrixMultp(int side, int16_t (&accumulator)[2][L1]) {
    auto *acc_us = (vector_type *) &accumulator[side][0];
    auto *acc_enemy = (vector_type *) &accumulator[!side][0];
    auto *weights = (vector_type *) &layer1_weights[0];
    auto zero = vector_set_zero();
    auto out = zero;

    for (int i = 0; i < L1 / (vector_size); i++) {
        out = vector_epi32_add(out, vector_multipy(vector_max(acc_us[i], zero), weights[i]));
    }
    const auto offset = L1/vector_size;
    for (int i = 0; i < L1 / (vector_size); i++) {
        out = vector_epi32_add(out, vector_multipy(vector_max(acc_enemy[i], zero), weights[i + offset]));
    }

    alignas(64) int32_t result[4];
#if defined(USE_AVX2)
    __m128i high_sum = _mm256_extractf128_si256(out, 1);
    __m128i low_sum = _mm256_castsi256_si128(out);
    *(__m128i *) &result[0] = _mm_add_epi32(high_sum, low_sum);
#else
    *(__m128i*) &result[0] = out;
#endif
    return (layer1_bias + result[0] + result[1] + result[2] + result[3]) / OUTPUT_DIVISOR;
}


void NNUE::recalculateInputLayer(Board &board, Color c) {
    int weightIndices[N_SQUARES];
    int sz = calculateIndices(board, weightIndices, c);

    auto *biases = (vector_type *) &feature_biases[0];
    auto *outputs = (vector_type *) &board.nnueData.accumulator[board.nnueData.size][c][0];  //white

    auto *weights = (vector_type *) &feature_weights[weightIndices[0]];
    for (int i = 0; i < L1 / vector_size; i++) {
        outputs[i] = vector_add(biases[i], weights[i]);
    }
    for (int k = 1; k < sz; k++) {
        weights = (vector_type *) &feature_weights[weightIndices[k]];
        for (int i = 0; i < L1 / vector_size; i++) {
            outputs[i] = vector_add(outputs[i], weights[i]);
        }
    }
}

void NNUE::calculateInputLayer(Board &board, bool fromScratch) {
    if (board.nnueData.size == 0 || fromScratch) {
        recalculateInputLayer(board, WHITE);
        recalculateInputLayer(board, BLACK);
    } else {
        uint16_t move = board.nnueData.move;
        int piece = board.pieceBoard[moveTo(move)];

        if (piece == BLACK_KING)
            recalculateInputLayer(board, BLACK);
        else {
            incrementalUpdate(board, BLACK);
        }

        if (piece == WHITE_KING)
            recalculateInputLayer(board, WHITE);
        else {
            incrementalUpdate(board, WHITE);
        }
    }
    board.nnueData.addAccumulator();
}

int NNUE::evaluate(Board &board) {
    int eval = quanMatrixMultp(board.sideToMove, board.nnueData.accumulator[board.nnueData.size - 1]);
    return eval * ((100.0 - board.halfMove) / 100.0);
}
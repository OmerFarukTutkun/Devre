#include "nnue.h"
#include "move.h"
#include <algorithm>

constexpr int nnueIndexMapping[2][14] = {{0, 1, 2, 3, 4,  5,  0, 0, 6, 7, 8, 9, 10, 11},
                                         {6, 7, 8, 9, 10, 11, 0, 0, 0, 1, 2, 3, 4,  5}};

#ifndef NET
#define NET "devre_06.08.2024.nnue"
#endif
INCBIN(EmbeddedWeights, NET);
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

    for (int16_t& bias: layer1_bias)
        bias = (*data_int16++);
}


int NNUE::nnueIndex(int king, int piece, int sq, int side) {
    if (side == BLACK)
        sq = mirrorVertically(sq);
    return nnueIndexMapping[side][piece] * 64 + sq;

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
int32_t NNUE::quanMatrixMultp(int16_t *us,int16_t *them, const int16_t *weights, const int16_t bias){
    auto zero = vector_set_zero();
    auto one = vector_set_epi16(QA);

    auto *vepi16Us = (vector_type *) &us[0];
    auto *vepi16Them = (vector_type *) &them[0];
    auto *vepi16Weights = (vector_type *) weights;
    auto out =vector_set_zero();

    int weightOffset = 0;
    for (const auto *acc : {vepi16Us, vepi16Them}) {
        for (int i = 0; i < L1/vector_size; i ++) {
            vector_type clipped = vector_min(one,  vector_max(zero, acc[i]));
            out = vector_epi32_add(out, vector_multipy(vector_mullo( clipped, clipped), vepi16Weights[i]));
        }

        vepi16Weights = (vector_type *) &weights[L1];
    }

    int32_t sum =0;
    sum = reduce_add_epi32(out);
    float constexpr scalar = NET_SCALE / (QA * QB);
    return (sum / QA + bias) * scalar;
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
         incrementalUpdate(board, WHITE);
         incrementalUpdate(board, BLACK);

    }
    board.nnueData.addAccumulator();
}

int NNUE::evaluate(Board &board) {
    auto us = board.nnueData.accumulator[board.nnueData.size - 1][board.sideToMove];
    auto enemy = board.nnueData.accumulator[board.nnueData.size - 1][1 - board.sideToMove];

    const int outputBucket = 0;


    int eval = quanMatrixMultp( us, enemy, &layer1_weights[2*L1*outputBucket], layer1_bias[outputBucket]);
    return eval * ((100.0 - board.halfMove) / 100.0);
}

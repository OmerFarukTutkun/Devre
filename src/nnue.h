#ifndef DEVRE_NNUE_H
#define DEVRE_NNUE_H

#include "board.h"
#include "incbin/incbin.h"
#include "types.h"


class NNUE {
   private:
    NNUE();

    alignas(64) int16_t feature_weights[INPUT_SIZE * L1];
    alignas(64) int16_t feature_biases[L1];
    alignas(64) int16_t layer1_weights[2 * L1 * OUTPUT_BUCKETS];
    alignas(64) int16_t layer1_bias[OUTPUT_BUCKETS];

    static int nnueIndex(int king, int piece, int sq, int side);

    static int calculateIndices(Board& board, int (&weightIndices)[N_SQUARES], Color c);

    //int32_t quanMatrixMultp(int side, int16_t (&accumulator)[2][L1]);

    void recalculateInputLayer(Board& board, Color c);

    void incrementalUpdate(Board& board, Color c);

   public:
    static float materialScale(Board& board);

    static float halfMoveScale(Board& board);

    void calculateInputLayer(Board& board, bool fromScratch = false);

    int evaluate(Board& board);

    static NNUE* Instance();

    static int32_t quanMatrixMultp(int16_t* us, int16_t* them, const int16_t* weights, int16_t bias);
};

#endif  //DEVRE_NNUE_H
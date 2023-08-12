#ifndef DEVRE_NNUE_H
#define DEVRE_NNUE_H

#include "board.h"
#include "incbin/incbin.h"
#include "types.h"

#if defined(USE_AVX2)

#include <immintrin.h>

#define vector_type      __m256i
#define vector_add       _mm256_add_epi16
#define vector_sub       _mm256_sub_epi16
#define vector_max       _mm256_max_epi16
#define vector_set_zero  _mm256_setzero_si256
#define vector_multipy   _mm256_madd_epi16
#define vector_epi32_add _mm256_add_epi32
constexpr int vector_size = 16;
#elif defined(USE_SSE3)
#include <tmmintrin.h>
#define vector_type      __m128i
#define vector_add       _mm_add_epi16
#define vector_sub       _mm_sub_epi16
#define vector_max       _mm_max_epi16
#define vector_set_zero  _mm_setzero_si128
#define vector_multipy   _mm_madd_epi16
#define vector_epi32_add _mm_add_epi32
constexpr int vector_size = 8;
#endif
class NNUE {
private:
    NNUE();

    alignas(64) int16_t feature_weights[INPUT_SIZE * L1];
    alignas(64) int16_t feature_biases[L1];
    alignas(64) int16_t layer1_weights[2 * L1];
    alignas(64) int32_t layer1_bias;

    static int nnueIndex(int king, int piece, int sq, int side);

    static int calculateIndices(Board &board, int (&weightIndices)[N_SQUARES], Color c);

    int32_t quanMatrixMultp(int side, int16_t (&accumulator)[2][L1]);

    void recalculateInputLayer(Board &board, Color c);

    void incrementalUpdate(Board &board, Color c);

public:

    void calculateInputLayer(Board &board, bool fromScratch = false);

    int evaluate(Board &board);

    static NNUE *Instance();
};

#endif //DEVRE_NNUE_H

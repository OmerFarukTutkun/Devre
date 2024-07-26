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
#define vector_min       _mm256_min_epi16
#define vector_max       _mm256_max_epi16
#define vector_set_zero  _mm256_setzero_si256
#define vector_multipy   _mm256_madd_epi16
#define vector_mullo     _mm256_mullo_epi16
#define vector_epi32_add _mm256_add_epi32
#define vector_set_epi16  _mm256_set1_epi16
constexpr int vector_size = 16;
#elif defined(USE_SSE3)
#include <tmmintrin.h>
#define vector_type      __m128i
#define vector_add       _mm_add_epi16
#define vector_sub       _mm_sub_epi16
#define vector_min       _mm_min_epi16
#define vector_max       _mm_max_epi16
#define vector_set_zero  _mm_setzero_si128
#define vector_multipy   _mm_madd_epi16
#define vector_mullo     _mm_mullo_epi16
#define vector_epi32_add _mm_add_epi32
#define vector_set_epi16 _mm_set1_epi16
constexpr int vector_size = 8;
#elif defined(USE_AVX512)
#include <immintrin.h>
#define vector_type      __m512i
#define vector_add       _mm512_add_epi16
#define vector_sub       _mm512_sub_epi16
#define vector_min       _mm512_min_epi16
#define vector_max       _mm512_max_epi16
#define vector_set_zero  _mm512_setzero_si512
#define vector_multipy   _mm512_madd_epi16
#define vector_mullo     _mm512_mullo_epi16
#define vector_epi32_add _mm512_add_epi32
#define vector_set_epi16  _mm512_set1_epi16
constexpr int vector_size = 32;
#endif

class NNUE {
private:
    NNUE();

    alignas(64) int16_t feature_weights[INPUT_SIZE * L1];
    alignas(64) int16_t feature_biases[L1];
    alignas(64) int16_t layer1_weights[2 * L1*OUTPUT_BUCKETS];
    alignas(64) int16_t layer1_bias[OUTPUT_BUCKETS];

    static int nnueIndex(int king, int piece, int sq, int side);

    static int calculateIndices(Board &board, int (&weightIndices)[N_SQUARES], Color c);

    //int32_t quanMatrixMultp(int side, int16_t (&accumulator)[2][L1]);

    void recalculateInputLayer(Board &board, Color c);

    void incrementalUpdate(Board &board, Color c);

    static void SCRelu(int16_t *array, int16_t *out, int size);
public:

    void calculateInputLayer(Board &board, bool fromScratch = false);

    int evaluate(Board &board);

    static NNUE *Instance();
    static int32_t quanMatrixMultp(int16_t *us, int16_t *them, const int16_t *weights, int16_t bias);
};

#endif //DEVRE_NNUE_H
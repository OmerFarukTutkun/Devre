#ifndef _NNUE_H_
#define _NNUE_H_

#include <stdalign.h>
#include "movegen.h"
#include "incbin/incbin.h"

#define INPUT_SIZE 768
#define L1 512
#define OUTPUT 1

#define SCALE 273
#define SCALE_WEIGHT 512 
#define WEIGHT_FILE "devre.nnue"

 #if defined(USE_AVX2)
    #include <immintrin.h>
    #define vector           __m256i
    #define vector_add       _mm256_add_epi16
    #define vector_sub       _mm256_sub_epi16
    #define vector_max       _mm256_max_epi16
    #define vector_set_zero  _mm256_setzero_si256
    #define vector_multipy   _mm256_madd_epi16
    #define vector_epi32_add _mm256_add_epi32
    #define vector_size      16
#elif defined(USE_SSE3)
    #include <tmmintrin.h>
    #define vector           __m128i
    #define vector_add       _mm_add_epi16
    #define vector_sub       _mm_sub_epi16
    #define vector_max       _mm_max_epi16
    #define vector_set_zero  _mm_setzero_si128
    #define vector_multipy   _mm_madd_epi16
    #define vector_epi32_add _mm_add_epi32
    #define vector_size      8
#endif
void set_weights();
int16_t evaluate_nnue(Position* pos);
#endif

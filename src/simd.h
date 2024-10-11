
#ifndef DEVRE_SIMD_H
#define DEVRE_SIMD_H


namespace SIMD {

#if defined(__AVX512F__) && defined(__AVX512BW__)

#include <immintrin.h>

    using vecType = __m512i;

    inline vecType vecAddEpi16(vecType a, vecType b) {
        return _mm512_add_epi16(a, b);
    }

    inline vecType vecSubEpi16(vecType a, vecType b) {
        return _mm512_sub_epi16(a, b);
    }

    inline vecType vecMaxEpi16(vecType a, vecType b) {
        return _mm512_max_epi16(a, b);
    }

    inline vecType vecMinEpi16(vecType a, vecType b) {
        return _mm512_min_epi16(a, b);
    }

    inline vecType vecSetZeroEpi16() {
        return _mm512_setzero_si512();
    }

    inline vecType vecMaddEpi16(vecType a, vecType b) {
        return _mm512_madd_epi16(a, b);
    }

    inline vecType vecMulloEpi16(vecType a, vecType b) {
        return _mm512_mullo_epi16(a, b);
    }

    inline vecType vecAddEpi32(vecType a, vecType b) {
        return _mm512_add_epi32(a, b);
    }

    inline vecType vecSetEpi16(int a) {
        return _mm512_set1_epi16(a);
    }

    inline int vecReduceEpi32(vecType a) {
        return _mm512_reduce_add_epi32(a);
    }

    inline int isVecZero(vecType a) {
        auto reslt = _mm512_test_epi16_mask(a, a);
        return (reslt == 0);
    }

    constexpr int vecSize = 32;
#elif defined(__AVX2__)
#include <immintrin.h>
    using   vecType = __m256i;

    inline vecType  vecAddEpi16(vecType a, vecType b)
    {
        return _mm256_add_epi16(a,b);
    }
    inline vecType  vecSubEpi16(vecType a, vecType b)
    {
        return _mm256_sub_epi16(a,b);
    }
    inline vecType  vecMaxEpi16(vecType a, vecType b)
    {
        return _mm256_max_epi16(a,b);
    }
    inline vecType  vecMinEpi16(vecType a, vecType b)
    {
        return _mm256_min_epi16(a,b);
    }
    inline vecType  vecSetZeroEpi16()
    {
        return _mm256_setzero_si256();
    }
    inline vecType  vecMaddEpi16(vecType a, vecType b)
    {
        return _mm256_madd_epi16(a,b);
    }
    inline vecType  vecMulloEpi16(vecType a, vecType b)
    {
        return _mm256_mullo_epi16(a,b);
    }
    inline vecType  vecAddEpi32(vecType a, vecType b)
    {
        return _mm256_add_epi32(a,b);
    }
    inline vecType  vecSetEpi16(int a)
    {
        return _mm256_set1_epi16(a);
    }
    inline int  vecReduceEpi32(vecType sum)
    {
        __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
        sum128         = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC));
        sum128         = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB));
        return _mm_cvtsi128_si32(sum128);
    }

    inline int isVecZero(vecType a)
    {
        auto reslt = _mm256_test_epi16_mask(a,a);
        return (reslt == 0);
    }
    constexpr int vecSize = 16;
#else
#include <tmmintrin.h>
    using   vecType = __m128i;
    inline vecType  vecAddEpi16(vecType a, vecType b)
    {
        return _mm_add_epi16(a,b);
    }
    inline vecType  vecSubEpi16(vecType a, vecType b)
    {
        return _mm_sub_epi16(a,b);
    }
    inline vecType  vecMaxEpi16(vecType a, vecType b)
    {
        return _mm_max_epi16(a,b);
    }
    inline vecType  vecMinEpi16(vecType a, vecType b)
    {
        return _mm_min_epi16(a,b);
    }
    inline vecType  vecSetZeroEpi16()
    {
        return _mm_setzero_si128();
    }
    inline vecType  vecMaddEpi16(vecType a, vecType b)
    {
        return _mm_madd_epi16(a,b);
    }
    inline vecType  vecMulloEpi16(vecType a, vecType b)
    {
        return _mm_mullo_epi16(a,b);
    }
    inline vecType  vecAddEpi32(vecType a, vecType b)
    {
        return _mm_add_epi32(a,b);
    }
    inline vecType  vecSetEpi16(int a)
    {
        return _mm_set1_epi16(a);
    }
    inline int  vecReduceEpi32(vecType sum)
    {
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4E));  //_MM_PERM_BADC
        sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xB1));  //_MM_PERM_CDAB
        return _mm_cvtsi128_si32(sum);
    }
    constexpr int vecSize = 8;

#endif
}

#endif //DEVRE_SIMD_H

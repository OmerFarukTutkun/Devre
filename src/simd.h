#ifndef DEVRE_SIMD_H
#define DEVRE_SIMD_H

#include <immintrin.h>
#include <cstdint>

namespace SIMD {

#if defined(DEVRE_ENABLE_AVX512) && defined(__AVX512F__) && defined(__AVX512BW__)

using vecType = __m512i;
constexpr int vecSize = 32;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm512_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm512_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm512_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm512_min_epi16(a, b); }
inline vecType vecZero() { return _mm512_setzero_si512(); }
inline vecType vecMaddEpi16(vecType a, vecType b) { return _mm512_madd_epi16(a, b); }
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm512_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm512_add_epi32(a, b); }
inline vecType vecSet1Epi16(int16_t a) { return _mm512_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm512_load_si512(reinterpret_cast<const void*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm512_store_si512(reinterpret_cast<void*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    return _mm512_cvtepi8_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(p)));
}
inline int vecReduceEpi32(vecType a) { return _mm512_reduce_add_epi32(a); }

#elif defined(__AVX2__)

using vecType = __m256i;
constexpr int vecSize = 16;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm256_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm256_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm256_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm256_min_epi16(a, b); }
inline vecType vecZero() { return _mm256_setzero_si256(); }
inline vecType vecMaddEpi16(vecType a, vecType b) { return _mm256_madd_epi16(a, b); }
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm256_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm256_add_epi32(a, b); }
inline vecType vecSet1Epi16(int16_t a) { return _mm256_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    return _mm256_cvtepi8_epi16(_mm_load_si128(reinterpret_cast<const __m128i*>(p)));
}
inline int vecReduceEpi32(vecType sum) {
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB));
    return _mm_cvtsi128_si32(sum128);
}

#else

using vecType = __m128i;
constexpr int vecSize = 8;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm_min_epi16(a, b); }
inline vecType vecZero() { return _mm_setzero_si128(); }
inline vecType vecMaddEpi16(vecType a, vecType b) { return _mm_madd_epi16(a, b); }
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm_add_epi32(a, b); }
inline vecType vecSet1Epi16(int16_t a) { return _mm_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    const __m128i bytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
    const __m128i sign = _mm_cmpgt_epi8(_mm_setzero_si128(), bytes);
    return _mm_unpacklo_epi8(bytes, sign);
}
inline int vecReduceEpi32(vecType sum) {
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4E));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xB1));
    return _mm_cvtsi128_si32(sum);
}

#endif
}

#endif  //DEVRE_SIMD_H


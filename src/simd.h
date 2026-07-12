#ifndef DEVRE_SIMD_H
#define DEVRE_SIMD_H

#include <immintrin.h>
#include <cstdint>

namespace SIMD {

#if defined(__AVX512F__) && defined(__AVX512BW__)

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

// Horizontal sums of eight vectors at once via a hadd tree; out[i] receives the
// lane sum of vector i. Wrap-around int32 addition makes the result identical
// to eight separate vecReduceEpi32 calls.
inline void vecReduceEpi32x8(vecType a0, vecType a1, vecType a2, vecType a3, vecType a4, vecType a5, vecType a6, vecType a7, int32_t* out) {
    const __m256i h0 = _mm256_add_epi32(_mm512_castsi512_si256(a0), _mm512_extracti64x4_epi64(a0, 1));
    const __m256i h1 = _mm256_add_epi32(_mm512_castsi512_si256(a1), _mm512_extracti64x4_epi64(a1, 1));
    const __m256i h2 = _mm256_add_epi32(_mm512_castsi512_si256(a2), _mm512_extracti64x4_epi64(a2, 1));
    const __m256i h3 = _mm256_add_epi32(_mm512_castsi512_si256(a3), _mm512_extracti64x4_epi64(a3, 1));
    const __m256i h4 = _mm256_add_epi32(_mm512_castsi512_si256(a4), _mm512_extracti64x4_epi64(a4, 1));
    const __m256i h5 = _mm256_add_epi32(_mm512_castsi512_si256(a5), _mm512_extracti64x4_epi64(a5, 1));
    const __m256i h6 = _mm256_add_epi32(_mm512_castsi512_si256(a6), _mm512_extracti64x4_epi64(a6, 1));
    const __m256i h7 = _mm256_add_epi32(_mm512_castsi512_si256(a7), _mm512_extracti64x4_epi64(a7, 1));

    const __m256i u0 = _mm256_hadd_epi32(_mm256_hadd_epi32(h0, h1), _mm256_hadd_epi32(h2, h3));
    const __m256i u1 = _mm256_hadd_epi32(_mm256_hadd_epi32(h4, h5), _mm256_hadd_epi32(h6, h7));

    const __m256i sums = _mm256_add_epi32(_mm256_permute2x128_si256(u0, u1, 0x20), _mm256_permute2x128_si256(u0, u1, 0x31));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out), sums);
}

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

// Horizontal sums of eight vectors at once via a hadd tree; out[i] receives the
// lane sum of vector i. Wrap-around int32 addition makes the result identical
// to eight separate vecReduceEpi32 calls.
inline void vecReduceEpi32x8(vecType a0, vecType a1, vecType a2, vecType a3, vecType a4, vecType a5, vecType a6, vecType a7, int32_t* out) {
    const __m256i u0 = _mm256_hadd_epi32(_mm256_hadd_epi32(a0, a1), _mm256_hadd_epi32(a2, a3));
    const __m256i u1 = _mm256_hadd_epi32(_mm256_hadd_epi32(a4, a5), _mm256_hadd_epi32(a6, a7));

    const __m256i sums = _mm256_add_epi32(_mm256_permute2x128_si256(u0, u1, 0x20), _mm256_permute2x128_si256(u0, u1, 0x31));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out), sums);
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

// SSE2 has no hadd; plain per-vector reductions keep this path simple.
inline void vecReduceEpi32x8(vecType a0, vecType a1, vecType a2, vecType a3, vecType a4, vecType a5, vecType a6, vecType a7, int32_t* out) {
    out[0] = vecReduceEpi32(a0);
    out[1] = vecReduceEpi32(a1);
    out[2] = vecReduceEpi32(a2);
    out[3] = vecReduceEpi32(a3);
    out[4] = vecReduceEpi32(a4);
    out[5] = vecReduceEpi32(a5);
    out[6] = vecReduceEpi32(a6);
    out[7] = vecReduceEpi32(a7);
}

#endif
}

#endif  //DEVRE_SIMD_H


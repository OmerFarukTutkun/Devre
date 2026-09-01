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
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm512_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm512_add_epi32(a, b); }
inline vecType vecSet1Epi16(int16_t a) { return _mm512_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm512_load_si512(reinterpret_cast<const void*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm512_store_si512(reinterpret_cast<void*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    return _mm512_cvtepi8_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(p)));
}
inline vecType vecBroadcastEpi32(int32_t a) { return _mm512_set1_epi32(a); }
inline void vecStoreEpi32(int32_t* p, vecType v) { _mm512_store_si512(reinterpret_cast<void*>(p), v); }
inline uint32_t nonZeroMaskEpi32(vecType v) { return _mm512_test_epi32_mask(v, v); }
inline vecType vecDpbusdEpi32(vecType acc, vecType a, vecType b) {
#if defined(__AVX512VNNI__)
    return _mm512_dpbusd_epi32(acc, a, b);
#else
    return _mm512_add_epi32(acc, _mm512_madd_epi16(_mm512_maddubs_epi16(a, b), _mm512_set1_epi16(1)));
#endif
}

// packUnsignedEpi16 keeps logical element order; the hardware pack interleaves
// 128-bit lanes, hence the permute.
template<int N>
inline vecType vecSrliEpi16(vecType a) { return _mm512_srli_epi16(a, N); }
inline vecType packUnsignedEpi16(vecType a, vecType b) {
    const __m512i order = _mm512_setr_epi64(0, 2, 4, 6, 1, 3, 5, 7);
    return _mm512_permutexvar_epi64(order, _mm512_packus_epi16(a, b));
}
inline vecType vecLoadRaw(const void* p) { return _mm512_load_si512(p); }
inline void vecStoreRaw(void* p, vecType v) { _mm512_store_si512(p, v); }

// --- float ops for the head -------------------------------------------------
using fvecType = __m512;
constexpr int fvecSize = 16;

inline fvecType fvecZero() { return _mm512_setzero_ps(); }
inline fvecType fvecSet1(float a) { return _mm512_set1_ps(a); }
inline fvecType fvecLoad(const float* p) { return _mm512_load_ps(p); }
inline void fvecStore(float* p, fvecType v) { _mm512_store_ps(p, v); }
inline fvecType fvecAdd(fvecType a, fvecType b) { return _mm512_add_ps(a, b); }
inline fvecType fvecMul(fvecType a, fvecType b) { return _mm512_mul_ps(a, b); }
inline fvecType fvecFmadd(fvecType a, fvecType b, fvecType c) { return _mm512_fmadd_ps(a, b, c); }
inline fvecType fvecMin(fvecType a, fvecType b) { return _mm512_min_ps(a, b); }
inline fvecType fvecMax(fvecType a, fvecType b) { return _mm512_max_ps(a, b); }
inline float fvecReduceAdd(fvecType v) { return _mm512_reduce_add_ps(v); }
inline int32_t vecReduceAddEpi32(vecType v) { return _mm512_reduce_add_epi32(v); }

#elif defined(__AVX2__)

using vecType = __m256i;
constexpr int vecSize = 16;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm256_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm256_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm256_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm256_min_epi16(a, b); }
inline vecType vecZero() { return _mm256_setzero_si256(); }
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm256_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm256_add_epi32(a, b); }
inline vecType vecSet1Epi16(int16_t a) { return _mm256_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    return _mm256_cvtepi8_epi16(_mm_load_si128(reinterpret_cast<const __m128i*>(p)));
}
inline vecType vecBroadcastEpi32(int32_t a) { return _mm256_set1_epi32(a); }
inline void vecStoreEpi32(int32_t* p, vecType v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
inline uint32_t nonZeroMaskEpi32(vecType v) {
    const __m256i isZero = _mm256_cmpeq_epi32(v, _mm256_setzero_si256());
    return static_cast<uint32_t>(~_mm256_movemask_ps(_mm256_castsi256_ps(isZero))) & 0xFFu;
}

template<int N>
inline vecType vecSrliEpi16(vecType a) { return _mm256_srli_epi16(a, N); }
inline vecType packUnsignedEpi16(vecType a, vecType b) {
    return _mm256_permute4x64_epi64(_mm256_packus_epi16(a, b), 0xD8);
}
inline vecType vecLoadRaw(const void* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
inline vecType vecDpbusdEpi32(vecType acc, vecType a, vecType b) {
#if (defined(__AVX512VNNI__) && defined(__AVX512VL__)) || defined(__AVXVNNI__)
    return _mm256_dpbusd_epi32(acc, a, b);
#else
    return _mm256_add_epi32(acc, _mm256_madd_epi16(_mm256_maddubs_epi16(a, b), _mm256_set1_epi16(1)));
#endif
}
inline void vecStoreRaw(void* p, vecType v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }

// --- float ops for the head -------------------------------------------------
using fvecType = __m256;
constexpr int fvecSize = 8;

inline fvecType fvecZero() { return _mm256_setzero_ps(); }
inline fvecType fvecSet1(float a) { return _mm256_set1_ps(a); }
inline fvecType fvecLoad(const float* p) { return _mm256_load_ps(p); }
inline void fvecStore(float* p, fvecType v) { _mm256_store_ps(p, v); }
inline fvecType fvecAdd(fvecType a, fvecType b) { return _mm256_add_ps(a, b); }
inline fvecType fvecMul(fvecType a, fvecType b) { return _mm256_mul_ps(a, b); }
inline fvecType fvecFmadd(fvecType a, fvecType b, fvecType c) {
#if defined(__FMA__)
    return _mm256_fmadd_ps(a, b, c);
#else
    return _mm256_add_ps(_mm256_mul_ps(a, b), c);
#endif
}
inline fvecType fvecMin(fvecType a, fvecType b) { return _mm256_min_ps(a, b); }
inline fvecType fvecMax(fvecType a, fvecType b) { return _mm256_max_ps(a, b); }
inline float fvecReduceAdd(fvecType v) {
    __m128 half = _mm_add_ps(_mm256_castps256_ps128(v), _mm256_extractf128_ps(v, 1));
    half = _mm_add_ps(half, _mm_movehl_ps(half, half));
    half = _mm_add_ss(half, _mm_shuffle_ps(half, half, 1));
    return _mm_cvtss_f32(half);
}
inline int32_t vecReduceAddEpi32(vecType v) {
    __m128i half = _mm_add_epi32(_mm256_castsi256_si128(v), _mm256_extracti128_si256(v, 1));
    half = _mm_add_epi32(half, _mm_srli_si128(half, 8));
    half = _mm_add_epi32(half, _mm_srli_si128(half, 4));
    return _mm_cvtsi128_si32(half);
}

#else

using vecType = __m128i;
constexpr int vecSize = 8;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm_min_epi16(a, b); }
inline vecType vecZero() { return _mm_setzero_si128(); }
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
inline vecType vecBroadcastEpi32(int32_t a) { return _mm_set1_epi32(a); }
inline void vecStoreEpi32(int32_t* p, vecType v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
inline uint32_t nonZeroMaskEpi32(vecType v) {
    const __m128i isZero = _mm_cmpeq_epi32(v, _mm_setzero_si128());
    return static_cast<uint32_t>(~_mm_movemask_ps(_mm_castsi128_ps(isZero))) & 0xFu;
}

template<int N>
inline vecType vecSrliEpi16(vecType a) { return _mm_srli_epi16(a, N); }
inline vecType packUnsignedEpi16(vecType a, vecType b) { return _mm_packus_epi16(a, b); }
inline vecType vecLoadRaw(const void* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
inline void vecStoreRaw(void* p, vecType v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }

// --- float ops for the head -------------------------------------------------
using fvecType = __m128;
constexpr int fvecSize = 4;

inline fvecType fvecZero() { return _mm_setzero_ps(); }
inline fvecType fvecSet1(float a) { return _mm_set1_ps(a); }
inline fvecType fvecLoad(const float* p) { return _mm_load_ps(p); }
inline void fvecStore(float* p, fvecType v) { _mm_store_ps(p, v); }
inline fvecType fvecAdd(fvecType a, fvecType b) { return _mm_add_ps(a, b); }
inline fvecType fvecMul(fvecType a, fvecType b) { return _mm_mul_ps(a, b); }
inline fvecType fvecFmadd(fvecType a, fvecType b, fvecType c) {
#if defined(__FMA__)
    return _mm_fmadd_ps(a, b, c);
#else
    return _mm_add_ps(_mm_mul_ps(a, b), c);
#endif
}
inline fvecType fvecMin(fvecType a, fvecType b) { return _mm_min_ps(a, b); }
inline fvecType fvecMax(fvecType a, fvecType b) { return _mm_max_ps(a, b); }
inline float fvecReduceAdd(fvecType v) {
    v = _mm_add_ps(v, _mm_movehl_ps(v, v));
    v = _mm_add_ss(v, _mm_shuffle_ps(v, v, 1));
    return _mm_cvtss_f32(v);
}
inline int32_t vecReduceAddEpi32(vecType v) {
    v = _mm_add_epi32(v, _mm_srli_si128(v, 8));
    v = _mm_add_epi32(v, _mm_srli_si128(v, 4));
    return _mm_cvtsi128_si32(v);
}
inline vecType vecDpbusdEpi32(vecType acc, vecType a, vecType b) {
#if defined(__SSSE3__)
    return _mm_add_epi32(acc, _mm_madd_epi16(_mm_maddubs_epi16(a, b), _mm_set1_epi16(1)));
#else
    // SSE2: widen to int16, madd gives two products per lane, the shuffle pair
    // folds adjacent lanes into the four a dpbusd lane is defined as.
    const __m128i zero = _mm_setzero_si128();
    const __m128i sign = _mm_cmpgt_epi8(zero, b);
    const __m128i lo   = _mm_madd_epi16(_mm_unpacklo_epi8(a, zero), _mm_unpacklo_epi8(b, sign));
    const __m128i hi   = _mm_madd_epi16(_mm_unpackhi_epi8(a, zero), _mm_unpackhi_epi8(b, sign));
    const __m128 lops = _mm_castsi128_ps(lo), hips = _mm_castsi128_ps(hi);
    const __m128i even = _mm_castps_si128(_mm_shuffle_ps(lops, hips, _MM_SHUFFLE(2, 0, 2, 0)));
    const __m128i odd  = _mm_castps_si128(_mm_shuffle_ps(lops, hips, _MM_SHUFFLE(3, 1, 3, 1)));
    return _mm_add_epi32(acc, _mm_add_epi32(even, odd));
#endif
}

#endif
}

#endif  //DEVRE_SIMD_H


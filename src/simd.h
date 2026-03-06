#ifndef DEVRE_SIMD_H
#define DEVRE_SIMD_H

#include <immintrin.h>
#include <cstdint>

namespace SIMD {

#if defined(DEVRE_ENABLE_AVX512) && defined(__AVX512F__) && defined(__AVX512BW__)

using vecType = __m512i;
constexpr int vecSize = 32;
using f32Type = __m512;
constexpr int f32Size = 16;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm512_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm512_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm512_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm512_min_epi16(a, b); }
inline vecType vecSetZeroEpi16() { return _mm512_setzero_si512(); }
inline vecType vecMaddEpi16(vecType a, vecType b) { return _mm512_madd_epi16(a, b); }
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm512_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm512_add_epi32(a, b); }
inline vecType vecSetEpi16(int a) { return _mm512_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm512_load_si512(reinterpret_cast<const void*>(p)); }
inline vecType vecLoadu(const int16_t* p) { return _mm512_loadu_si512(reinterpret_cast<const void*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm512_store_si512(reinterpret_cast<void*>(p), v); }
inline void vecStoreu(int16_t* p, vecType v) { _mm512_storeu_si512(reinterpret_cast<void*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    return _mm512_cvtepi8_epi16(_mm256_load_si256(reinterpret_cast<const __m256i*>(p)));
}
inline vecType vecLoaduI8ToI16(const int8_t* p) {
    return _mm512_cvtepi8_epi16(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)));
}
inline f32Type f32Add(f32Type a, f32Type b) { return _mm512_add_ps(a, b); }
inline f32Type f32Mul(f32Type a, f32Type b) { return _mm512_mul_ps(a, b); }
inline f32Type f32Min(f32Type a, f32Type b) { return _mm512_min_ps(a, b); }
inline f32Type f32Max(f32Type a, f32Type b) { return _mm512_max_ps(a, b); }
inline f32Type f32SetZero() { return _mm512_setzero_ps(); }
inline f32Type f32Set1(float a) { return _mm512_set1_ps(a); }
inline f32Type f32Load(const float* p) { return _mm512_load_ps(p); }
#ifdef __FMA__
inline f32Type f32Fmadd(f32Type a, f32Type b, f32Type c) { return _mm512_fmadd_ps(a, b, c); }
#else
inline f32Type f32Fmadd(f32Type a, f32Type b, f32Type c) { return _mm512_add_ps(_mm512_mul_ps(a, b), c); }
#endif
inline float f32ReduceAdd(f32Type sum) { return _mm512_reduce_add_ps(sum); }
inline int vecReduceEpi32(vecType a) { return _mm512_reduce_add_epi32(a); }
inline int isVecZero(vecType a) { return _mm512_cmpeq_epi32_mask(a, _mm512_setzero_si512()) == 0xFFFFu; }

#elif defined(__AVX2__)

using vecType = __m256i;
constexpr int vecSize = 16;
using f32Type = __m256;
constexpr int f32Size = 8;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm256_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm256_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm256_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm256_min_epi16(a, b); }
inline vecType vecSetZeroEpi16() { return _mm256_setzero_si256(); }
inline vecType vecMaddEpi16(vecType a, vecType b) { return _mm256_madd_epi16(a, b); }
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm256_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm256_add_epi32(a, b); }
inline vecType vecSetEpi16(int a) { return _mm256_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm256_load_si256(reinterpret_cast<const __m256i*>(p)); }
inline vecType vecLoadu(const int16_t* p) { return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm256_store_si256(reinterpret_cast<__m256i*>(p), v); }
inline void vecStoreu(int16_t* p, vecType v) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    return _mm256_cvtepi8_epi16(_mm_load_si128(reinterpret_cast<const __m128i*>(p)));
}
inline vecType vecLoaduI8ToI16(const int8_t* p) {
    return _mm256_cvtepi8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));
}
inline f32Type f32Add(f32Type a, f32Type b) { return _mm256_add_ps(a, b); }
inline f32Type f32Mul(f32Type a, f32Type b) { return _mm256_mul_ps(a, b); }
inline f32Type f32Min(f32Type a, f32Type b) { return _mm256_min_ps(a, b); }
inline f32Type f32Max(f32Type a, f32Type b) { return _mm256_max_ps(a, b); }
inline f32Type f32SetZero() { return _mm256_setzero_ps(); }
inline f32Type f32Set1(float a) { return _mm256_set1_ps(a); }
inline f32Type f32Load(const float* p) { return _mm256_load_ps(p); }
#ifdef __FMA__
inline f32Type f32Fmadd(f32Type a, f32Type b, f32Type c) { return _mm256_fmadd_ps(a, b, c); }
#else
inline f32Type f32Fmadd(f32Type a, f32Type b, f32Type c) { return _mm256_add_ps(_mm256_mul_ps(a, b), c); }
#endif
inline float f32ReduceAdd(f32Type sum) {
    __m128 sum128 = _mm_add_ps(_mm256_castps256_ps128(sum), _mm256_extractf128_ps(sum, 1));
    sum128 = _mm_add_ps(sum128, _mm_movehl_ps(sum128, sum128));
    sum128 = _mm_add_ss(sum128, _mm_shuffle_ps(sum128, sum128, 0x55));
    return _mm_cvtss_f32(sum128);
}
inline int vecReduceEpi32(vecType sum) {
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum), _mm256_extracti128_si256(sum, 1));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_BADC));
    sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, _MM_PERM_CDAB));
    return _mm_cvtsi128_si32(sum128);
}
inline int isVecZero(vecType a) { return _mm256_testz_si256(a, a); }

#else

using vecType = __m128i;
constexpr int vecSize = 8;
using f32Type = __m128;
constexpr int f32Size = 4;

inline vecType vecAddEpi16(vecType a, vecType b) { return _mm_add_epi16(a, b); }
inline vecType vecSubEpi16(vecType a, vecType b) { return _mm_sub_epi16(a, b); }
inline vecType vecMaxEpi16(vecType a, vecType b) { return _mm_max_epi16(a, b); }
inline vecType vecMinEpi16(vecType a, vecType b) { return _mm_min_epi16(a, b); }
inline vecType vecSetZeroEpi16() { return _mm_setzero_si128(); }
inline vecType vecMaddEpi16(vecType a, vecType b) { return _mm_madd_epi16(a, b); }
inline vecType vecMulloEpi16(vecType a, vecType b) { return _mm_mullo_epi16(a, b); }
inline vecType vecAddEpi32(vecType a, vecType b) { return _mm_add_epi32(a, b); }
inline vecType vecSetEpi16(int a) { return _mm_set1_epi16(a); }
inline vecType vecLoad(const int16_t* p) { return _mm_load_si128(reinterpret_cast<const __m128i*>(p)); }
inline vecType vecLoadu(const int16_t* p) { return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p)); }
inline void vecStore(int16_t* p, vecType v) { _mm_store_si128(reinterpret_cast<__m128i*>(p), v); }
inline void vecStoreu(int16_t* p, vecType v) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v); }
inline vecType vecLoadI8ToI16(const int8_t* p) {
    const __m128i bytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
    const __m128i sign = _mm_cmpgt_epi8(_mm_setzero_si128(), bytes);
    return _mm_unpacklo_epi8(bytes, sign);
}
inline vecType vecLoaduI8ToI16(const int8_t* p) {
    const __m128i bytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
    const __m128i sign = _mm_cmpgt_epi8(_mm_setzero_si128(), bytes);
    return _mm_unpacklo_epi8(bytes, sign);
}
inline f32Type f32Add(f32Type a, f32Type b) { return _mm_add_ps(a, b); }
inline f32Type f32Mul(f32Type a, f32Type b) { return _mm_mul_ps(a, b); }
inline f32Type f32Min(f32Type a, f32Type b) { return _mm_min_ps(a, b); }
inline f32Type f32Max(f32Type a, f32Type b) { return _mm_max_ps(a, b); }
inline f32Type f32SetZero() { return _mm_setzero_ps(); }
inline f32Type f32Set1(float a) { return _mm_set1_ps(a); }
inline f32Type f32Load(const float* p) { return _mm_load_ps(p); }
inline f32Type f32Fmadd(f32Type a, f32Type b, f32Type c) { return _mm_add_ps(_mm_mul_ps(a, b), c); }
inline float f32ReduceAdd(f32Type sum) {
    sum = _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
    sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 0x55));
    return _mm_cvtss_f32(sum);
}
inline int vecReduceEpi32(vecType sum) {
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4E));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xB1));
    return _mm_cvtsi128_si32(sum);
}
inline int isVecZero(vecType a) {
    return _mm_movemask_epi8(_mm_cmpeq_epi8(a, _mm_setzero_si128())) == 0xFFFF;
}

#endif
}

#endif  //DEVRE_SIMD_H

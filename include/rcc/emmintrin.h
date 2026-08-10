/*
 * RCC - emmintrin.h
 * SSE2 Intrinsics
 */

#ifndef _EMMINTRIN_H
#define _EMMINTRIN_H

#include <xmmintrin.h>

/* SSE2 data types */
typedef double __m128d __attribute__((__vector_size__(16)));
typedef long long __m128i __attribute__((__vector_size__(16)));

/* Load operations - double */
static __inline __m128d _mm_load_sd(const double* __p) {
    return __builtin_ia32_loadsd(__p);
}

static __inline __m128d _mm_load_pd(const double* __p) {
    return *(__m128d*)__p;
}

static __inline __m128d _mm_load1_pd(const double* __p) {
    return (__m128d){ *__p, *__p };
}

static __inline __m128d _mm_load_pd1(const double* __p) {
    return _mm_load1_pd(__p);
}

static __inline __m128d _mm_loadr_pd(const double* __p) {
    return (__m128d){ __p[1], __p[0] };
}

static __inline __m128d _mm_loadu_pd(const double* __p) {
    return __builtin_ia32_loadupd(__p);
}

/* Store operations - double */
static __inline void _mm_store_sd(double* __p, __m128d __a) {
    __builtin_ia32_storesd(__p, __a);
}

static __inline void _mm_store_pd(double* __p, __m128d __a) {
    *(__m128d*)__p = __a;
}

static __inline void _mm_store1_pd(double* __p, __m128d __a) {
    __p[0] = __p[1] = ((__v2df)__a)[0];
}

static __inline void _mm_store_pd1(double* __p, __m128d __a) {
    _mm_store1_pd(__p, __a);
}

static __inline void _mm_storer_pd(double* __p, __m128d __a) {
    __p[0] = ((__v2df)__a)[1];
    __p[1] = ((__v2df)__a)[0];
}

static __inline void _mm_storeu_pd(double* __p, __m128d __a) {
    __builtin_ia32_storeupd(__p, __a);
}

/* Arithmetic - double */
static __inline __m128d _mm_add_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_addsd(__a, __b);
}

static __inline __m128d _mm_add_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_addpd(__a, __b);
}

static __inline __m128d _mm_sub_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_subsd(__a, __b);
}

static __inline __m128d _mm_sub_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_subpd(__a, __b);
}

static __inline __m128d _mm_mul_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_mulsd(__a, __b);
}

static __inline __m128d _mm_mul_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_mulpd(__a, __b);
}

static __inline __m128d _mm_div_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_divsd(__a, __b);
}

static __inline __m128d _mm_div_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_divpd(__a, __b);
}

static __inline __m128d _mm_sqrt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_sqrtsd(__b);
}

static __inline __m128d _mm_sqrt_pd(__m128d __a) {
    return __builtin_ia32_sqrtpd(__a);
}

static __inline __m128d _mm_min_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_minsd(__a, __b);
}

static __inline __m128d _mm_min_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_minpd(__a, __b);
}

static __inline __m128d _mm_max_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_maxsd(__a, __b);
}

static __inline __m128d _mm_max_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_maxpd(__a, __b);
}

/* Logical - double */
static __inline __m128d _mm_and_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_andpd(__a, __b);
}

static __inline __m128d _mm_andnot_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_andnpd(__a, __b);
}

static __inline __m128d _mm_or_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_orpd(__a, __b);
}

static __inline __m128d _mm_xor_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_xorpd(__a, __b);
}

/* Comparison - double */
static __inline __m128d _mm_cmpeq_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpeqsd(__a, __b);
}

static __inline __m128d _mm_cmpeq_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpeqpd(__a, __b);
}

static __inline __m128d _mm_cmplt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpltsd(__a, __b);
}

static __inline __m128d _mm_cmplt_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpltpd(__a, __b);
}

static __inline __m128d _mm_cmple_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmplesd(__a, __b);
}

static __inline __m128d _mm_cmple_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmplepd(__a, __b);
}

static __inline __m128d _mm_cmpgt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpltsd(__b, __a);
}

static __inline __m128d _mm_cmpgt_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpltpd(__b, __a);
}

static __inline __m128d _mm_cmpge_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmplesd(__b, __a);
}

static __inline __m128d _mm_cmpge_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmplepd(__b, __a);
}

static __inline __m128d _mm_cmpneq_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpneqsd(__a, __b);
}

static __inline __m128d _mm_cmpneq_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpneqpd(__a, __b);
}

static __inline __m128d _mm_cmpord_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpordsd(__a, __b);
}

static __inline __m128d _mm_cmpord_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpordpd(__a, __b);
}

static __inline __m128d _mm_cmpunord_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpunordsd(__a, __b);
}

static __inline __m128d _mm_cmpunord_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpunordpd(__a, __b);
}

/* Scalar comparison - double */
static __inline int _mm_comieq_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_comisdeq(__a, __b);
}

static __inline int _mm_comilt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_comisdlt(__a, __b);
}

static __inline int _mm_comile_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_comisdle(__a, __b);
}

static __inline int _mm_comigt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_comisdgt(__a, __b);
}

static __inline int _mm_comige_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_comisdge(__a, __b);
}

static __inline int _mm_comineq_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_comisdneq(__a, __b);
}

/* Set operations - double */
static __inline __m128d _mm_set_sd(double __w) {
    return (__m128d){ __w, 0 };
}

static __inline __m128d _mm_set1_pd(double __w) {
    return (__m128d){ __w, __w };
}

static __inline __m128d _mm_set_pd1(double __w) {
    return _mm_set1_pd(__w);
}

static __inline __m128d _mm_set_pd(double __w, double __x) {
    return (__m128d){ __x, __w };
}

static __inline __m128d _mm_setr_pd(double __w, double __x) {
    return (__m128d){ __w, __x };
}

static __inline __m128d _mm_setzero_pd(void) {
    return (__m128d){ 0.0, 0.0 };
}

/* Shuffle - double */
#define _mm_shuffle_pd(a, b, imm) \
    __builtin_ia32_shufpd(a, b, imm)

/* Unpack - double */
static __inline __m128d _mm_unpackhi_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_unpckhpd(__a, __b);
}

static __inline __m128d _mm_unpacklo_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_unpcklpd(__a, __b);
}

/* Move - double */
static __inline __m128d _mm_move_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_movsd(__a, __b);
}

static __inline int _mm_movemask_pd(__m128d __a) {
    return __builtin_ia32_movmskpd(__a);
}

/* Integer operations */
static __inline __m128i _mm_add_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_add_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_add_epi32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddd128((__v4si)__a, (__v4si)__b);
}

static __inline __m128i _mm_add_epi64(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddq128((__v2di)__a, (__v2di)__b);
}

static __inline __m128i _mm_sub_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_sub_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_sub_epi32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubd128((__v4si)__a, (__v4si)__b);
}

static __inline __m128i _mm_sub_epi64(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubq128((__v2di)__a, (__v2di)__b);
}

static __inline __m128i _mm_mullo_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pmullw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_mulhi_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pmulhw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_mulhi_epu16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pmulhuw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_mul_epu32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pmuludq128((__v4si)__a, (__v4si)__b);
}

/* Logical */
static __inline __m128i _mm_and_si128(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pand128((__v2di)__a, (__v2di)__b);
}

static __inline __m128i _mm_andnot_si128(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pandn128((__v2di)__a, (__v2di)__b);
}

static __inline __m128i _mm_or_si128(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_por128((__v2di)__a, (__v2di)__b);
}

static __inline __m128i _mm_xor_si128(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pxor128((__v2di)__a, (__v2di)__b);
}

/* Shift */
static __inline __m128i _mm_slli_epi16(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_psllwi128((__v8hi)__a, __count);
}

static __inline __m128i _mm_slli_epi32(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_pslldi128((__v4si)__a, __count);
}

static __inline __m128i _mm_slli_epi64(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_psllqi128((__v2di)__a, __count);
}

static __inline __m128i _mm_srli_epi16(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_psrlwi128((__v8hi)__a, __count);
}

static __inline __m128i _mm_srli_epi32(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_psrldi128((__v4si)__a, __count);
}

static __inline __m128i _mm_srli_epi64(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_psrlqi128((__v2di)__a, __count);
}

static __inline __m128i _mm_srai_epi16(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_psrawi128((__v8hi)__a, __count);
}

static __inline __m128i _mm_srai_epi32(__m128i __a, int __count) {
    return (__m128i)__builtin_ia32_psradi128((__v4si)__a, __count);
}

/* Comparison */
static __inline __m128i _mm_cmpeq_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pcmpeqb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_cmpeq_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pcmpeqw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_cmpeq_epi32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pcmpeqd128((__v4si)__a, (__v4si)__b);
}

static __inline __m128i _mm_cmpgt_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pcmpgtb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_cmpgt_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pcmpgtw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_cmpgt_epi32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pcmpgtd128((__v4si)__a, (__v4si)__b);
}

static __inline __m128i _mm_cmplt_epi8(__m128i __a, __m128i __b) {
    return _mm_cmpgt_epi8(__b, __a);
}

static __inline __m128i _mm_cmplt_epi16(__m128i __a, __m128i __b) {
    return _mm_cmpgt_epi16(__b, __a);
}

static __inline __m128i _mm_cmplt_epi32(__m128i __a, __m128i __b) {
    return _mm_cmpgt_epi32(__b, __a);
}

/* Pack/Unpack */
static __inline __m128i _mm_packs_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_packsswb128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_packs_epi32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_packssdw128((__v4si)__a, (__v4si)__b);
}

static __inline __m128i _mm_packus_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_packuswb128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_unpackhi_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpckhbw128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_unpackhi_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpckhwd128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_unpackhi_epi32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpckhdq128((__v4si)__a, (__v4si)__b);
}

static __inline __m128i _mm_unpackhi_epi64(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpckhqdq128((__v2di)__a, (__v2di)__b);
}

static __inline __m128i _mm_unpacklo_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpcklbw128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_unpacklo_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpcklwd128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_unpacklo_epi32(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpckldq128((__v4si)__a, (__v4si)__b);
}

static __inline __m128i _mm_unpacklo_epi64(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_punpcklqdq128((__v2di)__a, (__v2di)__b);
}

/* Set operations - integer */
static __inline __m128i _mm_set_epi64x(long long __q1, long long __q0) {
    return (__m128i){ __q0, __q1 };
}

static __inline __m128i _mm_set_epi32(int __q3, int __q2, int __q1, int __q0) {
    return (__m128i)(__v4si){ __q0, __q1, __q2, __q3 };
}

static __inline __m128i _mm_set_epi16(short __q7, short __q6, short __q5, short __q4,
                                       short __q3, short __q2, short __q1, short __q0) {
    return (__m128i)(__v8hi){ __q0, __q1, __q2, __q3, __q4, __q5, __q6, __q7 };
}

static __inline __m128i _mm_set_epi8(char __q15, char __q14, char __q13, char __q12,
                                      char __q11, char __q10, char __q9, char __q8,
                                      char __q7, char __q6, char __q5, char __q4,
                                      char __q3, char __q2, char __q1, char __q0) {
    return (__m128i)(__v16qi){ __q0, __q1, __q2, __q3, __q4, __q5, __q6, __q7,
                               __q8, __q9, __q10, __q11, __q12, __q13, __q14, __q15 };
}

static __inline __m128i _mm_set1_epi64x(long long __q) {
    return _mm_set_epi64x(__q, __q);
}

static __inline __m128i _mm_set1_epi32(int __q) {
    return _mm_set_epi32(__q, __q, __q, __q);
}

static __inline __m128i _mm_set1_epi16(short __q) {
    return _mm_set_epi16(__q, __q, __q, __q, __q, __q, __q, __q);
}

static __inline __m128i _mm_set1_epi8(char __q) {
    return _mm_set_epi8(__q, __q, __q, __q, __q, __q, __q, __q,
                        __q, __q, __q, __q, __q, __q, __q, __q);
}

static __inline __m128i _mm_setzero_si128(void) {
    return (__m128i){ 0LL, 0LL };
}

/* Load/Store - integer */
static __inline __m128i _mm_load_si128(const __m128i* __p) {
    return *__p;
}

static __inline __m128i _mm_loadu_si128(const __m128i* __p) {
    return __builtin_ia32_loaddqu((const char*)__p);
}

static __inline void _mm_store_si128(__m128i* __p, __m128i __a) {
    *__p = __a;
}

static __inline void _mm_storeu_si128(__m128i* __p, __m128i __a) {
    __builtin_ia32_storedqu((char*)__p, (__v16qi)__a);
}

/* Movemask */
static __inline int _mm_movemask_epi8(__m128i __a) {
    return __builtin_ia32_pmovmskb128((__v16qi)__a);
}

/* Shuffle */
#define _mm_shuffle_epi32(a, imm) \
    ((__m128i)__builtin_ia32_pshufd((__v4si)(a), (imm)))

#define _mm_shufflelo_epi16(a, imm) \
    ((__m128i)__builtin_ia32_pshuflw((__v8hi)(a), (imm)))

#define _mm_shufflehi_epi16(a, imm) \
    ((__m128i)__builtin_ia32_pshufhw((__v8hi)(a), (imm)))

/* Extract/Insert */
#define _mm_extract_epi16(a, imm) \
    ((int)(unsigned short)__builtin_ia32_vec_ext_v8hi((__v8hi)(a), (imm)))

#define _mm_insert_epi16(a, i, imm) \
    ((__m128i)__builtin_ia32_vec_set_v8hi((__v8hi)(a), (i), (imm)))

/* Conversion */
static __inline __m128 _mm_cvtepi32_ps(__m128i __a) {
    return __builtin_ia32_cvtdq2ps((__v4si)__a);
}

static __inline __m128d _mm_cvtepi32_pd(__m128i __a) {
    return __builtin_ia32_cvtdq2pd((__v4si)__a);
}

static __inline __m128i _mm_cvtps_epi32(__m128 __a) {
    return (__m128i)__builtin_ia32_cvtps2dq(__a);
}

static __inline __m128i _mm_cvttpd_epi32(__m128d __a) {
    return (__m128i)__builtin_ia32_cvttpd2dq(__a);
}

static __inline __m128i _mm_cvttps_epi32(__m128 __a) {
    return (__m128i)__builtin_ia32_cvttps2dq(__a);
}

static __inline __m128d _mm_cvtps_pd(__m128 __a) {
    return __builtin_ia32_cvtps2pd(__a);
}

static __inline __m128 _mm_cvtpd_ps(__m128d __a) {
    return __builtin_ia32_cvtpd2ps(__a);
}

/* Memory fence */
static __inline void _mm_lfence(void) {
    __builtin_ia32_lfence();
}

static __inline void _mm_mfence(void) {
    __builtin_ia32_mfence();
}

/* Stream operations */
static __inline void _mm_stream_si128(__m128i* __p, __m128i __a) {
    __builtin_ia32_movntdq(__p, __a);
}

static __inline void _mm_stream_pd(double* __p, __m128d __a) {
    __builtin_ia32_movntpd(__p, __a);
}

static __inline void _mm_stream_si32(int* __p, int __a) {
    __builtin_ia32_movnti(__p, __a);
}

/* Pause instruction */
static __inline void _mm_pause(void) {
    __builtin_ia32_pause();
}

/* Flush cache line */
static __inline void _mm_clflush(const void* __p) {
    __builtin_ia32_clflush(__p);
}

#endif /* _EMMINTRIN_H */

/*
 * RCC - emmintrin.h
 * SSE2 Intrinsics
 */

#ifndef _EMMINTRIN_H
#define _EMMINTRIN_H

#include <xmmintrin.h>

typedef long long __m128i_u __attribute__((__vector_size__(16),
                                           __may_alias__, __aligned__(1)));
typedef unsigned long long __rcc_v2du
    __attribute__((__vector_size__(16), __may_alias__));
typedef unsigned short __rcc_unaligned_u16
    __attribute__((__may_alias__, __aligned__(1)));
typedef unsigned int __rcc_unaligned_u32
    __attribute__((__may_alias__, __aligned__(1)));
typedef unsigned long long __rcc_unaligned_u64
    __attribute__((__may_alias__, __aligned__(1)));

#define _mm_castpd_ps(__a) ((__m128)(__a))
#define _mm_castpd_si128(__a) ((__m128i)(__a))
#define _mm_castps_pd(__a) ((__m128d)(__a))
#define _mm_castps_si128(__a) ((__m128i)(__a))
#define _mm_castsi128_pd(__a) ((__m128d)(__a))
#define _mm_castsi128_ps(__a) ((__m128)(__a))

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

static __inline __m128d _mm_loadl_pd(__m128d __a, const double* __p) {
    return (__m128d){ *__p, ((__v2df)__a)[1] };
}

static __inline __m128d _mm_loadh_pd(__m128d __a, const double* __p) {
    return (__m128d){ ((__v2df)__a)[0], *__p };
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

static __inline void _mm_storel_pd(double* __p, __m128d __a) {
    _mm_store_sd(__p, __a);
}

static __inline void _mm_storeh_pd(double* __p, __m128d __a) {
    *__p = ((__v2df)__a)[1];
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

static __inline __m128d _mm_cmpnge_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_movsd(
        __a, __builtin_ia32_cmpnlesd(__b, __a));
}

static __inline __m128d _mm_cmpnge_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpnlepd(__b, __a);
}

static __inline __m128d _mm_cmpngt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_movsd(
        __a, __builtin_ia32_cmpnltsd(__b, __a));
}

static __inline __m128d _mm_cmpngt_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpnltpd(__b, __a);
}

static __inline __m128d _mm_cmpneq_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpneqsd(__a, __b);
}

static __inline __m128d _mm_cmpneq_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpneqpd(__a, __b);
}

static __inline __m128d _mm_cmpnlt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpnltsd(__a, __b);
}

static __inline __m128d _mm_cmpnlt_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpnltpd(__a, __b);
}

static __inline __m128d _mm_cmpnle_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpnlesd(__a, __b);
}

static __inline __m128d _mm_cmpnle_pd(__m128d __a, __m128d __b) {
    return __builtin_ia32_cmpnlepd(__a, __b);
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

static __inline int _mm_ucomieq_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_ucomisdeq(__a, __b);
}

static __inline int _mm_ucomilt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_ucomisdlt(__a, __b);
}

static __inline int _mm_ucomile_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_ucomisdle(__a, __b);
}

static __inline int _mm_ucomigt_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_ucomisdgt(__a, __b);
}

static __inline int _mm_ucomige_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_ucomisdge(__a, __b);
}

static __inline int _mm_ucomineq_sd(__m128d __a, __m128d __b) {
    return __builtin_ia32_ucomisdneq(__a, __b);
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

static __inline __m128i _mm_adds_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddsb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_adds_epu8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddusb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_add_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_adds_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddsw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_adds_epu16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_paddusw128((__v8hi)__a, (__v8hi)__b);
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

static __inline __m128i _mm_subs_epi8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubsb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_subs_epu8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubusb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_sub_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_subs_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubsw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_subs_epu16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psubusw128((__v8hi)__a, (__v8hi)__b);
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

static __inline __m128i _mm_madd_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pmaddwd128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_avg_epu8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pavgb128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_avg_epu16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pavgw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_max_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pmaxsw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_max_epu8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pmaxub128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_min_epi16(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pminsw128((__v8hi)__a, (__v8hi)__b);
}

static __inline __m128i _mm_min_epu8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_pminub128((__v16qi)__a, (__v16qi)__b);
}

static __inline __m128i _mm_sad_epu8(__m128i __a, __m128i __b) {
    return (__m128i)__builtin_ia32_psadbw128((__v16qi)__a, (__v16qi)__b);
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

static __inline __m128i _mm_sll_epi16(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_psllw128((__v8hi)__a, (__v8hi)__count);
}

static __inline __m128i _mm_sll_epi32(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_pslld128((__v4si)__a, (__v4si)__count);
}

static __inline __m128i _mm_sll_epi64(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_psllq128((__v2di)__a, (__v2di)__count);
}

static __inline __m128i _mm_sra_epi16(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_psraw128((__v8hi)__a, (__v8hi)__count);
}

static __inline __m128i _mm_sra_epi32(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_psrad128((__v4si)__a, (__v4si)__count);
}

static __inline __m128i _mm_srl_epi16(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_psrlw128((__v8hi)__a, (__v8hi)__count);
}

static __inline __m128i _mm_srl_epi32(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_psrld128((__v4si)__a, (__v4si)__count);
}

static __inline __m128i _mm_srl_epi64(__m128i __a, __m128i __count) {
    return (__m128i)__builtin_ia32_psrlq128((__v2di)__a, (__v2di)__count);
}

#define _mm_bslli_si128(__a, __count) \
    ((__m128i)__builtin_ia32_pslldqi128( \
        (__m128i)(__a), (int)(__count) * 8))
#define _mm_bsrli_si128(__a, __count) \
    ((__m128i)__builtin_ia32_psrldqi128( \
        (__m128i)(__a), (int)(__count) * 8))
#define _mm_slli_si128(__a, __count) _mm_bslli_si128((__a), (__count))
#define _mm_srli_si128(__a, __count) _mm_bsrli_si128((__a), (__count))

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

static __inline __m128i _mm_set_epi64(__m64 __q1, __m64 __q0) {
    return (__m128i)(__rcc_v2du){ (unsigned long long)__q0,
                                  (unsigned long long)__q1 };
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

static __inline __m128i _mm_setr_epi32(int __q0, int __q1, int __q2, int __q3) {
    return _mm_set_epi32(__q3, __q2, __q1, __q0);
}

static __inline __m128i _mm_setr_epi16(short __q0, short __q1, short __q2,
                                       short __q3, short __q4, short __q5,
                                       short __q6, short __q7) {
    return _mm_set_epi16(__q7, __q6, __q5, __q4, __q3, __q2, __q1, __q0);
}

static __inline __m128i _mm_setr_epi8(char __q00, char __q01, char __q02,
                                      char __q03, char __q04, char __q05,
                                      char __q06, char __q07, char __q08,
                                      char __q09, char __q10, char __q11,
                                      char __q12, char __q13, char __q14,
                                      char __q15) {
    return _mm_set_epi8(__q15, __q14, __q13, __q12, __q11, __q10, __q09,
                        __q08, __q07, __q06, __q05, __q04, __q03, __q02,
                        __q01, __q00);
}

static __inline __m128i _mm_set1_epi64x(long long __q) {
    return _mm_set_epi64x(__q, __q);
}

static __inline __m128i _mm_set1_epi64(__m64 __q) {
    return _mm_set_epi64(__q, __q);
}

static __inline __m128i _mm_setr_epi64(__m64 __q0, __m64 __q1) {
    return _mm_set_epi64(__q1, __q0);
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

static __inline __m128i _mm_undefined_si128(void) {
    return _mm_setzero_si128();
}

static __inline __m128i _mm_move_epi64(__m128i __a) {
    return (__m128i)__builtin_ia32_movq128((__v2di)__a);
}

static __inline __m64 _mm_movepi64_pi64(__m128i __a) {
    return (__m64)__builtin_ia32_movdq2q((__v2di)__a);
}

static __inline __m128i _mm_movpi64_epi64(__m64 __a) {
    return (__m128i)__builtin_ia32_movq2dq(__a);
}

static __inline __m64 _mm_mul_su32(__m64 __a, __m64 __b) {
    return (__m64)__builtin_ia32_pmuludq(__a, __b);
}

static __inline __m128d _mm_undefined_pd(void) {
    return _mm_setzero_pd();
}

static __inline __m128i _mm_cvtsi32_si128(int __a) {
    return _mm_set_epi32(0, 0, 0, __a);
}

static __inline __m128i _mm_cvtsi64_si128(long long __a) {
    return _mm_set_epi64x(0, __a);
}

static __inline int _mm_cvtsi128_si32(__m128i __a) {
    return ((__v4si)__a)[0];
}

static __inline long long _mm_cvtsi128_si64(__m128i __a) {
    return ((__v2di)__a)[0];
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

static __inline void _mm_maskmoveu_si128(__m128i __a, __m128i __mask,
                                         char* __p) {
    __builtin_ia32_maskmovdqu((__v16qi)__a, (__v16qi)__mask, __p);
}

static __inline __m128i _mm_loadu_si16(const void* __p) {
    unsigned long long __low = (unsigned long long)
        *(const __rcc_unaligned_u16*)__p;
    return (__m128i)(__rcc_v2du){ __low, 0ULL };
}

static __inline __m128i _mm_loadu_si32(const void* __p) {
    unsigned long long __low = (unsigned long long)
        *(const __rcc_unaligned_u32*)__p;
    return (__m128i)(__rcc_v2du){ __low, 0ULL };
}

static __inline __m128i _mm_loadu_si64(const void* __p) {
    unsigned long long __low = *(const __rcc_unaligned_u64*)__p;
    return (__m128i)(__rcc_v2du){ __low, 0ULL };
}

static __inline __m128i _mm_loadl_epi64(const __m128i_u* __p) {
    return _mm_loadu_si64((const void*)__p);
}

static __inline void _mm_storeu_si16(void* __p, __m128i __a) {
    *(__rcc_unaligned_u16*)__p = (unsigned short)((__v8hi)__a)[0];
}

static __inline void _mm_storeu_si32(void* __p, __m128i __a) {
    *(__rcc_unaligned_u32*)__p = (unsigned int)((__v4si)__a)[0];
}

static __inline void _mm_storeu_si64(void* __p, __m128i __a) {
    *(__rcc_unaligned_u64*)__p = ((__rcc_v2du)__a)[0];
}

static __inline void _mm_storel_epi64(__m128i_u* __p, __m128i __a) {
    _mm_storeu_si64((void*)__p, __a);
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

static __inline __m128i _mm_cvtpd_epi32(__m128d __a) {
    return (__m128i)__builtin_ia32_cvtpd2dq(__a);
}

static __inline __m64 _mm_cvtpd_pi32(__m128d __a) {
    return (__m64)__builtin_ia32_cvtpd2pi((__v2df)__a);
}

static __inline __m64 _mm_cvttpd_pi32(__m128d __a) {
    return (__m64)__builtin_ia32_cvttpd2pi((__v2df)__a);
}

static __inline __m128d _mm_cvtpi32_pd(__m64 __a) {
    return (__m128d)__builtin_ia32_cvtpi2pd(__a);
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

static __inline __m128 _mm_cvtsd_ss(__m128 __a, __m128d __b) {
    return (__m128)__builtin_ia32_cvtsd2ss((__v4sf)__a, (__v2df)__b);
}

static __inline __m128d _mm_cvtss_sd(__m128d __a, __m128 __b) {
    return (__m128d)__builtin_ia32_cvtss2sd((__v2df)__a, (__v4sf)__b);
}

static __inline __m128d _mm_cvtsi32_sd(__m128d __a, int __b) {
    return __builtin_ia32_cvtsi2sd((__v2df)__a, __b);
}

#ifdef __x86_64__
static __inline __m128d _mm_cvtsi64_sd(__m128d __a, long long __b) {
    return __builtin_ia32_cvtsi642sd((__v2df)__a, __b);
}

#define _mm_cvtsi64x_sd _mm_cvtsi64_sd
#endif

static __inline double _mm_cvtsd_f64(__m128d __a) {
    return ((__v2df)__a)[0];
}

static __inline int _mm_cvtsd_si32(__m128d __a) {
    return __builtin_ia32_cvtsd2si(__a);
}

static __inline int _mm_cvttsd_si32(__m128d __a) {
    return __builtin_ia32_cvttsd2si(__a);
}

#ifdef __x86_64__
static __inline long long _mm_cvtsd_si64(__m128d __a) {
    return __builtin_ia32_cvtsd2si64(__a);
}

static __inline long long _mm_cvttsd_si64(__m128d __a) {
    return __builtin_ia32_cvttsd2si64(__a);
}

#define _mm_cvtsd_si64x _mm_cvtsd_si64
#define _mm_cvttsd_si64x _mm_cvttsd_si64
#define _mm_cvtsi128_si64x _mm_cvtsi128_si64
#define _mm_cvtsi64x_si128 _mm_cvtsi64_si128
#endif

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

#ifdef __x86_64__
static __inline void _mm_stream_si64(long long* __p, long long __a) {
    __builtin_ia32_movnti64(__p, __a);
}
#endif

/* Pause instruction */
static __inline void _mm_pause(void) {
    __builtin_ia32_pause();
}

/* Flush cache line */
static __inline void _mm_clflush(const void* __p) {
    __builtin_ia32_clflush(__p);
}

#endif /* _EMMINTRIN_H */

/*
 * RCC - xmmintrin.h
 * SSE Intrinsics
 */

#ifndef _XMMINTRIN_H
#define _XMMINTRIN_H

#include <mmintrin.h>

/* SSE data type: 128-bit packed single-precision float */
typedef float __m128 __attribute__((__vector_size__(16)));

/* Typedefs for integer interpretation */
typedef long long __m128i __attribute__((__vector_size__(16)));
typedef double __m128d __attribute__((__vector_size__(16)));

/* Load operations */
static __inline __m128 _mm_load_ss(const float* __p) {
    return __builtin_ia32_loadss(__p);
}

static __inline __m128 _mm_load_ps(const float* __p) {
    return *(__m128*)__p;
}

static __inline __m128 _mm_load_ps1(const float* __p) {
    return __builtin_ia32_loadss(__p);
}

static __inline __m128 _mm_load1_ps(const float* __p) {
    return _mm_load_ps1(__p);
}

static __inline __m128 _mm_loadr_ps(const float* __p) {
    __m128 __a = _mm_load_ps(__p);
    return __builtin_ia32_shufps(__a, __a, 0x1B);
}

static __inline __m128 _mm_loadu_ps(const float* __p) {
    return __builtin_ia32_loadups(__p);
}

/* Store operations */
static __inline void _mm_store_ss(float* __p, __m128 __a) {
    __builtin_ia32_storess(__p, __a);
}

static __inline void _mm_store_ps(float* __p, __m128 __a) {
    *(__m128*)__p = __a;
}

static __inline void _mm_store_ps1(float* __p, __m128 __a) {
    __p[0] = __p[1] = __p[2] = __p[3] = ((__v4sf)__a)[0];
}

static __inline void _mm_store1_ps(float* __p, __m128 __a) {
    _mm_store_ps1(__p, __a);
}

static __inline void _mm_storer_ps(float* __p, __m128 __a) {
    _mm_store_ps(__p, __builtin_ia32_shufps(__a, __a, 0x1B));
}

static __inline void _mm_storeu_ps(float* __p, __m128 __a) {
    __builtin_ia32_storeups(__p, __a);
}

/* Arithmetic operations */
static __inline __m128 _mm_add_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_addss(__a, __b);
}

static __inline __m128 _mm_add_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_addps(__a, __b);
}

static __inline __m128 _mm_sub_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_subss(__a, __b);
}

static __inline __m128 _mm_sub_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_subps(__a, __b);
}

static __inline __m128 _mm_mul_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_mulss(__a, __b);
}

static __inline __m128 _mm_mul_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_mulps(__a, __b);
}

static __inline __m128 _mm_div_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_divss(__a, __b);
}

static __inline __m128 _mm_div_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_divps(__a, __b);
}

static __inline __m128 _mm_sqrt_ss(__m128 __a) {
    return __builtin_ia32_sqrtss(__a);
}

static __inline __m128 _mm_sqrt_ps(__m128 __a) {
    return __builtin_ia32_sqrtps(__a);
}

static __inline __m128 _mm_rcp_ss(__m128 __a) {
    return __builtin_ia32_rcpss(__a);
}

static __inline __m128 _mm_rcp_ps(__m128 __a) {
    return __builtin_ia32_rcpps(__a);
}

static __inline __m128 _mm_rsqrt_ss(__m128 __a) {
    return __builtin_ia32_rsqrtss(__a);
}

static __inline __m128 _mm_rsqrt_ps(__m128 __a) {
    return __builtin_ia32_rsqrtps(__a);
}

static __inline __m128 _mm_min_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_minss(__a, __b);
}

static __inline __m128 _mm_min_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_minps(__a, __b);
}

static __inline __m128 _mm_max_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_maxss(__a, __b);
}

static __inline __m128 _mm_max_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_maxps(__a, __b);
}

/* Logical operations */
static __inline __m128 _mm_and_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_andps(__a, __b);
}

static __inline __m128 _mm_andnot_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_andnps(__a, __b);
}

static __inline __m128 _mm_or_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_orps(__a, __b);
}

static __inline __m128 _mm_xor_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_xorps(__a, __b);
}

/* Comparison operations */
static __inline __m128 _mm_cmpeq_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpeqss(__a, __b);
}

static __inline __m128 _mm_cmpeq_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpeqps(__a, __b);
}

static __inline __m128 _mm_cmplt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpltss(__a, __b);
}

static __inline __m128 _mm_cmplt_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpltps(__a, __b);
}

static __inline __m128 _mm_cmple_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpless(__a, __b);
}

static __inline __m128 _mm_cmple_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpleps(__a, __b);
}

static __inline __m128 _mm_cmpgt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpltss(__b, __a);
}

static __inline __m128 _mm_cmpgt_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpltps(__b, __a);
}

static __inline __m128 _mm_cmpge_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpless(__b, __a);
}

static __inline __m128 _mm_cmpge_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpleps(__b, __a);
}

static __inline __m128 _mm_cmpneq_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpneqss(__a, __b);
}

static __inline __m128 _mm_cmpneq_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpneqps(__a, __b);
}

static __inline __m128 _mm_cmpnlt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnltss(__a, __b);
}

static __inline __m128 _mm_cmpnlt_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnltps(__a, __b);
}

static __inline __m128 _mm_cmpnle_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnless(__a, __b);
}

static __inline __m128 _mm_cmpnle_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnleps(__a, __b);
}

static __inline __m128 _mm_cmpngt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnltss(__b, __a);
}

static __inline __m128 _mm_cmpngt_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnltps(__b, __a);
}

static __inline __m128 _mm_cmpnge_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnless(__b, __a);
}

static __inline __m128 _mm_cmpnge_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpnleps(__b, __a);
}

static __inline __m128 _mm_cmpord_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpordss(__a, __b);
}

static __inline __m128 _mm_cmpord_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpordps(__a, __b);
}

static __inline __m128 _mm_cmpunord_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpunordss(__a, __b);
}

static __inline __m128 _mm_cmpunord_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_cmpunordps(__a, __b);
}

/* Scalar comparison returning int */
static __inline int _mm_comieq_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_comieq(__a, __b);
}

static __inline int _mm_comilt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_comilt(__a, __b);
}

static __inline int _mm_comile_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_comile(__a, __b);
}

static __inline int _mm_comigt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_comigt(__a, __b);
}

static __inline int _mm_comige_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_comige(__a, __b);
}

static __inline int _mm_comineq_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_comineq(__a, __b);
}

static __inline int _mm_ucomieq_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_ucomieq(__a, __b);
}

static __inline int _mm_ucomilt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_ucomilt(__a, __b);
}

static __inline int _mm_ucomile_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_ucomile(__a, __b);
}

static __inline int _mm_ucomigt_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_ucomigt(__a, __b);
}

static __inline int _mm_ucomige_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_ucomige(__a, __b);
}

static __inline int _mm_ucomineq_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_ucomineq(__a, __b);
}

/* Conversion */
static __inline int _mm_cvt_ss2si(__m128 __a) {
    return __builtin_ia32_cvtss2si(__a);
}

static __inline int _mm_cvtss_si32(__m128 __a) {
    return _mm_cvt_ss2si(__a);
}

#ifdef __x86_64__
static __inline long long _mm_cvtss_si64(__m128 __a) {
    return __builtin_ia32_cvtss2si64(__a);
}
#endif

static __inline __m128 _mm_cvt_si2ss(__m128 __a, int __b) {
    return __builtin_ia32_cvtsi2ss(__a, __b);
}

static __inline __m128 _mm_cvtsi32_ss(__m128 __a, int __b) {
    return _mm_cvt_si2ss(__a, __b);
}

#ifdef __x86_64__
static __inline __m128 _mm_cvtsi64_ss(__m128 __a, long long __b) {
    return __builtin_ia32_cvtsi642ss(__a, __b);
}
#endif

static __inline int _mm_cvtt_ss2si(__m128 __a) {
    return __builtin_ia32_cvttss2si(__a);
}

static __inline int _mm_cvttss_si32(__m128 __a) {
    return _mm_cvtt_ss2si(__a);
}

#ifdef __x86_64__
static __inline long long _mm_cvttss_si64(__m128 __a) {
    return __builtin_ia32_cvttss2si64(__a);
}
#endif

/* Set operations */
static __inline __m128 _mm_set_ss(float __w) {
    return __builtin_ia32_loadss(&__w);
}

static __inline __m128 _mm_set_ps1(float __w) {
    return (__m128){ __w, __w, __w, __w };
}

static __inline __m128 _mm_set1_ps(float __w) {
    return _mm_set_ps1(__w);
}

static __inline __m128 _mm_set_ps(float __z, float __y, float __x, float __w) {
    return (__m128){ __w, __x, __y, __z };
}

static __inline __m128 _mm_setr_ps(float __w, float __x, float __y, float __z) {
    return (__m128){ __w, __x, __y, __z };
}

static __inline __m128 _mm_setzero_ps(void) {
    return (__m128){ 0.0f, 0.0f, 0.0f, 0.0f };
}

/* Shuffle */
#define _MM_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))

#define _mm_shuffle_ps(a, b, imm) \
    __builtin_ia32_shufps(a, b, imm)

/* Unpack */
static __inline __m128 _mm_unpackhi_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_unpckhps(__a, __b);
}

static __inline __m128 _mm_unpacklo_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_unpcklps(__a, __b);
}

/* Move */
static __inline __m128 _mm_move_ss(__m128 __a, __m128 __b) {
    return __builtin_ia32_movss(__a, __b);
}

static __inline __m128 _mm_movehl_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_movhlps(__a, __b);
}

static __inline __m128 _mm_movelh_ps(__m128 __a, __m128 __b) {
    return __builtin_ia32_movlhps(__a, __b);
}

static __inline int _mm_movemask_ps(__m128 __a) {
    return __builtin_ia32_movmskps(__a);
}

/* Prefetch */
#define _MM_HINT_T0  3
#define _MM_HINT_T1  2
#define _MM_HINT_T2  1
#define _MM_HINT_NTA 0

static __inline void _mm_prefetch(const void* __p, int __i) {
    __builtin_prefetch(__p, 0, __i);
}

/* Cache control */
static __inline void _mm_stream_pi(__m64* __p, __m64 __a) {
    __builtin_ia32_movntq(__p, __a);
}

static __inline void _mm_stream_ps(float* __p, __m128 __a) {
    __builtin_ia32_movntps(__p, __a);
}

/* Memory fence */
static __inline void _mm_sfence(void) {
    __builtin_ia32_sfence();
}

/* Extract scalar */
static __inline float _mm_cvtss_f32(__m128 __a) {
    return ((__v4sf)__a)[0];
}

/* MXCSR control */
static __inline unsigned int _mm_getcsr(void) {
    return __builtin_ia32_stmxcsr();
}

static __inline void _mm_setcsr(unsigned int __i) {
    __builtin_ia32_ldmxcsr(__i);
}

/* MXCSR bits */
#define _MM_EXCEPT_INVALID    0x0001
#define _MM_EXCEPT_DENORM     0x0002
#define _MM_EXCEPT_DIV_ZERO   0x0004
#define _MM_EXCEPT_OVERFLOW   0x0008
#define _MM_EXCEPT_UNDERFLOW  0x0010
#define _MM_EXCEPT_INEXACT    0x0020
#define _MM_EXCEPT_MASK       0x003f

#define _MM_MASK_INVALID      0x0080
#define _MM_MASK_DENORM       0x0100
#define _MM_MASK_DIV_ZERO     0x0200
#define _MM_MASK_OVERFLOW     0x0400
#define _MM_MASK_UNDERFLOW    0x0800
#define _MM_MASK_INEXACT      0x1000
#define _MM_MASK_MASK         0x1f80

#define _MM_ROUND_NEAREST     0x0000
#define _MM_ROUND_DOWN        0x2000
#define _MM_ROUND_UP          0x4000
#define _MM_ROUND_TOWARD_ZERO 0x6000
#define _MM_ROUND_MASK        0x6000

#define _MM_FLUSH_ZERO_MASK   0x8000
#define _MM_FLUSH_ZERO_ON     0x8000
#define _MM_FLUSH_ZERO_OFF    0x0000

#define _MM_SET_EXCEPTION_MASK(x) _mm_setcsr((_mm_getcsr() & ~_MM_MASK_MASK) | (x))
#define _MM_GET_EXCEPTION_MASK() (_mm_getcsr() & _MM_MASK_MASK)
#define _MM_SET_EXCEPTION_STATE(x) _mm_setcsr((_mm_getcsr() & ~_MM_EXCEPT_MASK) | (x))
#define _MM_GET_EXCEPTION_STATE() (_mm_getcsr() & _MM_EXCEPT_MASK)
#define _MM_SET_ROUNDING_MODE(x) _mm_setcsr((_mm_getcsr() & ~_MM_ROUND_MASK) | (x))
#define _MM_GET_ROUNDING_MODE() (_mm_getcsr() & _MM_ROUND_MASK)
#define _MM_SET_FLUSH_ZERO_MODE(x) _mm_setcsr((_mm_getcsr() & ~_MM_FLUSH_ZERO_MASK) | (x))
#define _MM_GET_FLUSH_ZERO_MODE() (_mm_getcsr() & _MM_FLUSH_ZERO_MASK)

/* Transpose macro */
#define _MM_TRANSPOSE4_PS(row0, row1, row2, row3) do { \
    __m128 __t0 = _mm_unpacklo_ps(row0, row1); \
    __m128 __t1 = _mm_unpacklo_ps(row2, row3); \
    __m128 __t2 = _mm_unpackhi_ps(row0, row1); \
    __m128 __t3 = _mm_unpackhi_ps(row2, row3); \
    row0 = _mm_movelh_ps(__t0, __t1); \
    row1 = _mm_movehl_ps(__t1, __t0); \
    row2 = _mm_movelh_ps(__t2, __t3); \
    row3 = _mm_movehl_ps(__t3, __t2); \
} while (0)

#endif /* _XMMINTRIN_H */

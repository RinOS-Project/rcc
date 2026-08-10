/*
 * RCC - mmintrin.h
 * MMX Intrinsics
 */

#ifndef _MMINTRIN_H
#define _MMINTRIN_H

/* MMX data type: 64-bit packed integer */
typedef long long __m64 __attribute__((__vector_size__(8)));

/* Empty MMX state */
static __inline void _mm_empty(void) {
    __asm__ __volatile__("emms");
}

static __inline void _m_empty(void) {
    _mm_empty();
}

/* Pack with signed saturation */
static __inline __m64 _mm_packs_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_packsswb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_packs_pi32(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_packssdw((__m64)__m1, (__m64)__m2);
}

/* Pack with unsigned saturation */
static __inline __m64 _mm_packs_pu16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_packuswb((__m64)__m1, (__m64)__m2);
}

/* Unpack and interleave */
static __inline __m64 _mm_unpackhi_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_punpckhbw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_unpackhi_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_punpckhwd((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_unpackhi_pi32(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_punpckhdq((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_unpacklo_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_punpcklbw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_unpacklo_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_punpcklwd((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_unpacklo_pi32(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_punpckldq((__m64)__m1, (__m64)__m2);
}

/* Integer arithmetic */
static __inline __m64 _mm_add_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_paddb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_add_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_paddw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_add_pi32(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_paddd((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_adds_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_paddsb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_adds_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_paddsw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_adds_pu8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_paddusb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_adds_pu16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_paddusw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_sub_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_psubb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_sub_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_psubw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_sub_pi32(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_psubd((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_subs_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_psubsb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_subs_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_psubsw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_subs_pu8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_psubusb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_subs_pu16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_psubusw((__m64)__m1, (__m64)__m2);
}

/* Multiply */
static __inline __m64 _mm_madd_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pmaddwd((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_mulhi_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pmulhw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_mullo_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pmullw((__m64)__m1, (__m64)__m2);
}

/* Shift */
static __inline __m64 _mm_sll_pi16(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_psllw((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_sll_pi32(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_pslld((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_sll_si64(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_psllq((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_slli_pi16(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_psllwi((__m64)__m, __count);
}

static __inline __m64 _mm_slli_pi32(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_pslldi((__m64)__m, __count);
}

static __inline __m64 _mm_slli_si64(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_psllqi((__m64)__m, __count);
}

static __inline __m64 _mm_sra_pi16(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_psraw((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_sra_pi32(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_psrad((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_srai_pi16(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_psrawi((__m64)__m, __count);
}

static __inline __m64 _mm_srai_pi32(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_psradi((__m64)__m, __count);
}

static __inline __m64 _mm_srl_pi16(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_psrlw((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_srl_pi32(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_psrld((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_srl_si64(__m64 __m, __m64 __count) {
    return (__m64)__builtin_ia32_psrlq((__m64)__m, (__m64)__count);
}

static __inline __m64 _mm_srli_pi16(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_psrlwi((__m64)__m, __count);
}

static __inline __m64 _mm_srli_pi32(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_psrldi((__m64)__m, __count);
}

static __inline __m64 _mm_srli_si64(__m64 __m, int __count) {
    return (__m64)__builtin_ia32_psrlqi((__m64)__m, __count);
}

/* Logical */
static __inline __m64 _mm_and_si64(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pand((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_andnot_si64(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pandn((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_or_si64(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_por((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_xor_si64(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pxor((__m64)__m1, (__m64)__m2);
}

/* Compare */
static __inline __m64 _mm_cmpeq_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pcmpeqb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_cmpeq_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pcmpeqw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_cmpeq_pi32(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pcmpeqd((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_cmpgt_pi8(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pcmpgtb((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_cmpgt_pi16(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pcmpgtw((__m64)__m1, (__m64)__m2);
}

static __inline __m64 _mm_cmpgt_pi32(__m64 __m1, __m64 __m2) {
    return (__m64)__builtin_ia32_pcmpgtd((__m64)__m1, (__m64)__m2);
}

/* Set/Create */
static __inline __m64 _mm_setzero_si64(void) {
    return (__m64)0LL;
}

static __inline __m64 _mm_set_pi32(int __i1, int __i0) {
    return (__m64)__builtin_ia32_vec_init_v2si(__i0, __i1);
}

static __inline __m64 _mm_set_pi16(short __w3, short __w2, short __w1, short __w0) {
    return (__m64)__builtin_ia32_vec_init_v4hi(__w0, __w1, __w2, __w3);
}

static __inline __m64 _mm_set_pi8(char __b7, char __b6, char __b5, char __b4,
                                   char __b3, char __b2, char __b1, char __b0) {
    return (__m64)__builtin_ia32_vec_init_v8qi(__b0, __b1, __b2, __b3, __b4, __b5, __b6, __b7);
}

static __inline __m64 _mm_set1_pi32(int __i) {
    return _mm_set_pi32(__i, __i);
}

static __inline __m64 _mm_set1_pi16(short __w) {
    return _mm_set_pi16(__w, __w, __w, __w);
}

static __inline __m64 _mm_set1_pi8(char __b) {
    return _mm_set_pi8(__b, __b, __b, __b, __b, __b, __b, __b);
}

static __inline __m64 _mm_setr_pi32(int __i0, int __i1) {
    return _mm_set_pi32(__i1, __i0);
}

static __inline __m64 _mm_setr_pi16(short __w0, short __w1, short __w2, short __w3) {
    return _mm_set_pi16(__w3, __w2, __w1, __w0);
}

static __inline __m64 _mm_setr_pi8(char __b0, char __b1, char __b2, char __b3,
                                    char __b4, char __b5, char __b6, char __b7) {
    return _mm_set_pi8(__b7, __b6, __b5, __b4, __b3, __b2, __b1, __b0);
}

/* Aliases */
#define _m_packsswb _mm_packs_pi16
#define _m_packssdw _mm_packs_pi32
#define _m_packuswb _mm_packs_pu16
#define _m_punpckhbw _mm_unpackhi_pi8
#define _m_punpckhwd _mm_unpackhi_pi16
#define _m_punpckhdq _mm_unpackhi_pi32
#define _m_punpcklbw _mm_unpacklo_pi8
#define _m_punpcklwd _mm_unpacklo_pi16
#define _m_punpckldq _mm_unpacklo_pi32
#define _m_paddb _mm_add_pi8
#define _m_paddw _mm_add_pi16
#define _m_paddd _mm_add_pi32
#define _m_paddsb _mm_adds_pi8
#define _m_paddsw _mm_adds_pi16
#define _m_paddusb _mm_adds_pu8
#define _m_paddusw _mm_adds_pu16
#define _m_psubb _mm_sub_pi8
#define _m_psubw _mm_sub_pi16
#define _m_psubd _mm_sub_pi32
#define _m_psubsb _mm_subs_pi8
#define _m_psubsw _mm_subs_pi16
#define _m_psubusb _mm_subs_pu8
#define _m_psubusw _mm_subs_pu16
#define _m_pmaddwd _mm_madd_pi16
#define _m_pmulhw _mm_mulhi_pi16
#define _m_pmullw _mm_mullo_pi16
#define _m_psllw _mm_sll_pi16
#define _m_pslld _mm_sll_pi32
#define _m_psllq _mm_sll_si64
#define _m_psllwi _mm_slli_pi16
#define _m_pslldi _mm_slli_pi32
#define _m_psllqi _mm_slli_si64
#define _m_psraw _mm_sra_pi16
#define _m_psrad _mm_sra_pi32
#define _m_psrawi _mm_srai_pi16
#define _m_psradi _mm_srai_pi32
#define _m_psrlw _mm_srl_pi16
#define _m_psrld _mm_srl_pi32
#define _m_psrlq _mm_srl_si64
#define _m_psrlwi _mm_srli_pi16
#define _m_psrldi _mm_srli_pi32
#define _m_psrlqi _mm_srli_si64
#define _m_pand _mm_and_si64
#define _m_pandn _mm_andnot_si64
#define _m_por _mm_or_si64
#define _m_pxor _mm_xor_si64
#define _m_pcmpeqb _mm_cmpeq_pi8
#define _m_pcmpeqw _mm_cmpeq_pi16
#define _m_pcmpeqd _mm_cmpeq_pi32
#define _m_pcmpgtb _mm_cmpgt_pi8
#define _m_pcmpgtw _mm_cmpgt_pi16
#define _m_pcmpgtd _mm_cmpgt_pi32

#endif /* _MMINTRIN_H */

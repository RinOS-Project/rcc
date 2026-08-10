/*
 * RCC - x86intrin.h
 * Combined x86 Intrinsics Header
 */

#ifndef _X86INTRIN_H
#define _X86INTRIN_H

#include <mmintrin.h>    /* MMX */
#include <xmmintrin.h>   /* SSE */
#include <emmintrin.h>   /* SSE2 */

/* Additional intrinsics for common builtins */

/* Bit manipulation */
static __inline int __bsfd(int __x) {
    return __builtin_ctz(__x);
}

static __inline int __bsrd(int __x) {
    return 31 - __builtin_clz(__x);
}

#ifdef __x86_64__
static __inline int __bsfq(long long __x) {
    return __builtin_ctzll(__x);
}

static __inline int __bsrq(long long __x) {
    return 63 - __builtin_clzll(__x);
}
#endif

/* Population count */
static __inline int __popcntd(unsigned int __x) {
    return __builtin_popcount(__x);
}

#ifdef __x86_64__
static __inline long long __popcntq(unsigned long long __x) {
    return __builtin_popcountll(__x);
}
#endif

/* Byte swap */
static __inline unsigned short __bswap16(unsigned short __x) {
    return __builtin_bswap16(__x);
}

static __inline unsigned int __bswap32(unsigned int __x) {
    return __builtin_bswap32(__x);
}

static __inline unsigned long long __bswap64(unsigned long long __x) {
    return __builtin_bswap64(__x);
}

/* Rotate */
static __inline unsigned int __rold(unsigned int __x, int __y) {
    return (__x << __y) | (__x >> (32 - __y));
}

static __inline unsigned int __rord(unsigned int __x, int __y) {
    return (__x >> __y) | (__x << (32 - __y));
}

#ifdef __x86_64__
static __inline unsigned long long __rolq(unsigned long long __x, int __y) {
    return (__x << __y) | (__x >> (64 - __y));
}

static __inline unsigned long long __rorq(unsigned long long __x, int __y) {
    return (__x >> __y) | (__x << (64 - __y));
}
#endif

/* Read timestamp counter */
static __inline unsigned long long __rdtsc(void) {
    unsigned int __lo, __hi;
    __asm__ __volatile__("rdtsc" : "=a"(__lo), "=d"(__hi));
    return ((unsigned long long)__hi << 32) | __lo;
}

static __inline unsigned long long __rdtscp(unsigned int* __p) {
    unsigned int __lo, __hi;
    __asm__ __volatile__("rdtscp" : "=a"(__lo), "=d"(__hi), "=c"(*__p));
    return ((unsigned long long)__hi << 32) | __lo;
}

/* CPUID */
static __inline void __cpuid(int __info[4], int __level) {
    __asm__ __volatile__("cpuid"
        : "=a"(__info[0]), "=b"(__info[1]), "=c"(__info[2]), "=d"(__info[3])
        : "a"(__level), "c"(0));
}

static __inline void __cpuidex(int __info[4], int __level, int __ecx) {
    __asm__ __volatile__("cpuid"
        : "=a"(__info[0]), "=b"(__info[1]), "=c"(__info[2]), "=d"(__info[3])
        : "a"(__level), "c"(__ecx));
}

#endif /* _X86INTRIN_H */

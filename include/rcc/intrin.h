/*
 * RCC - intrin.h
 * MSVC-compatible intrinsics header
 */

#ifndef _INTRIN_H
#define _INTRIN_H

#include <x86intrin.h>

/* MSVC-compatible intrinsic aliases */

/* Bit scanning */
static __inline unsigned char _BitScanForward(unsigned long* _Index, unsigned long _Mask) {
    if (_Mask == 0) return 0;
    *_Index = __builtin_ctz(_Mask);
    return 1;
}

static __inline unsigned char _BitScanReverse(unsigned long* _Index, unsigned long _Mask) {
    if (_Mask == 0) return 0;
    *_Index = 31 - __builtin_clz(_Mask);
    return 1;
}

#ifdef __x86_64__
static __inline unsigned char _BitScanForward64(unsigned long* _Index, unsigned long long _Mask) {
    if (_Mask == 0) return 0;
    *_Index = __builtin_ctzll(_Mask);
    return 1;
}

static __inline unsigned char _BitScanReverse64(unsigned long* _Index, unsigned long long _Mask) {
    if (_Mask == 0) return 0;
    *_Index = 63 - __builtin_clzll(_Mask);
    return 1;
}
#endif

/* Byte swap */
static __inline unsigned short _byteswap_ushort(unsigned short _x) {
    return __builtin_bswap16(_x);
}

static __inline unsigned long _byteswap_ulong(unsigned long _x) {
    return __builtin_bswap32(_x);
}

static __inline unsigned long long _byteswap_uint64(unsigned long long _x) {
    return __builtin_bswap64(_x);
}

/* Rotate */
static __inline unsigned char _rotl8(unsigned char _x, unsigned char _y) {
    return (_x << _y) | (_x >> (8 - _y));
}

static __inline unsigned short _rotl16(unsigned short _x, unsigned char _y) {
    return (_x << _y) | (_x >> (16 - _y));
}

static __inline unsigned int _rotl(unsigned int _x, int _y) {
    return (_x << _y) | (_x >> (32 - _y));
}

static __inline unsigned long long _rotl64(unsigned long long _x, int _y) {
    return (_x << _y) | (_x >> (64 - _y));
}

static __inline unsigned char _rotr8(unsigned char _x, unsigned char _y) {
    return (_x >> _y) | (_x << (8 - _y));
}

static __inline unsigned short _rotr16(unsigned short _x, unsigned char _y) {
    return (_x >> _y) | (_x << (16 - _y));
}

static __inline unsigned int _rotr(unsigned int _x, int _y) {
    return (_x >> _y) | (_x << (32 - _y));
}

static __inline unsigned long long _rotr64(unsigned long long _x, int _y) {
    return (_x >> _y) | (_x << (64 - _y));
}

/* Population count */
static __inline unsigned int __popcnt(unsigned int _x) {
    return __builtin_popcount(_x);
}

static __inline unsigned short __popcnt16(unsigned short _x) {
    return __builtin_popcount(_x);
}

#ifdef __x86_64__
static __inline unsigned long long __popcnt64(unsigned long long _x) {
    return __builtin_popcountll(_x);
}
#endif

/* Leading zeros count */
static __inline unsigned int __lzcnt(unsigned int _x) {
    return __builtin_clz(_x);
}

static __inline unsigned short __lzcnt16(unsigned short _x) {
    return __builtin_clz(_x) - 16;
}

#ifdef __x86_64__
static __inline unsigned long long __lzcnt64(unsigned long long _x) {
    return __builtin_clzll(_x);
}
#endif

/* Debug break */
static __inline void __debugbreak(void) {
    __asm__ __volatile__("int3");
}

/* Compiler barrier */
static __inline void _ReadWriteBarrier(void) {
    __asm__ __volatile__("" ::: "memory");
}

static __inline void _ReadBarrier(void) {
    __asm__ __volatile__("" ::: "memory");
}

static __inline void _WriteBarrier(void) {
    __asm__ __volatile__("" ::: "memory");
}

/* Memory fence */
static __inline void __faststorefence(void) {
    __asm__ __volatile__("lock; orl $0, (%%rsp)" ::: "memory");
}

/* Halt */
static __inline void __halt(void) {
    __asm__ __volatile__("hlt");
}

/* NOP */
static __inline void __nop(void) {
    __asm__ __volatile__("nop");
}

/* Read/Write MSR */
static __inline unsigned long long __readmsr(unsigned long _Register) {
    unsigned int __lo, __hi;
    __asm__ __volatile__("rdmsr" : "=a"(__lo), "=d"(__hi) : "c"(_Register));
    return ((unsigned long long)__hi << 32) | __lo;
}

static __inline void __writemsr(unsigned long _Register, unsigned long long _Value) {
    __asm__ __volatile__("wrmsr" :: "c"(_Register),
                         "a"((unsigned int)_Value),
                         "d"((unsigned int)(_Value >> 32)));
}

/* Read/Write CR registers */
static __inline unsigned long long __readcr0(void) {
    unsigned long long __val;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(__val));
    return __val;
}

static __inline unsigned long long __readcr2(void) {
    unsigned long long __val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(__val));
    return __val;
}

static __inline unsigned long long __readcr3(void) {
    unsigned long long __val;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(__val));
    return __val;
}

static __inline unsigned long long __readcr4(void) {
    unsigned long long __val;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(__val));
    return __val;
}

static __inline void __writecr0(unsigned long long _Val) {
    __asm__ __volatile__("mov %0, %%cr0" :: "r"(_Val));
}

static __inline void __writecr3(unsigned long long _Val) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(_Val));
}

static __inline void __writecr4(unsigned long long _Val) {
    __asm__ __volatile__("mov %0, %%cr4" :: "r"(_Val));
}

/* I/O port operations */
static __inline unsigned char __inbyte(unsigned short _Port) {
    unsigned char __val;
    __asm__ __volatile__("inb %w1, %b0" : "=a"(__val) : "Nd"(_Port));
    return __val;
}

static __inline unsigned short __inword(unsigned short _Port) {
    unsigned short __val;
    __asm__ __volatile__("inw %w1, %w0" : "=a"(__val) : "Nd"(_Port));
    return __val;
}

static __inline unsigned long __indword(unsigned short _Port) {
    unsigned long __val;
    __asm__ __volatile__("inl %w1, %k0" : "=a"(__val) : "Nd"(_Port));
    return __val;
}

static __inline void __outbyte(unsigned short _Port, unsigned char _Val) {
    __asm__ __volatile__("outb %b0, %w1" :: "a"(_Val), "Nd"(_Port));
}

static __inline void __outword(unsigned short _Port, unsigned short _Val) {
    __asm__ __volatile__("outw %w0, %w1" :: "a"(_Val), "Nd"(_Port));
}

static __inline void __outdword(unsigned short _Port, unsigned long _Val) {
    __asm__ __volatile__("outl %k0, %w1" :: "a"(_Val), "Nd"(_Port));
}

/* CPUID */
static __inline void __cpuid(int _Info[4], int _Type) {
    __asm__ __volatile__("cpuid"
        : "=a"(_Info[0]), "=b"(_Info[1]), "=c"(_Info[2]), "=d"(_Info[3])
        : "a"(_Type), "c"(0));
}

static __inline void __cpuidex(int _Info[4], int _Type, int _SubType) {
    __asm__ __volatile__("cpuid"
        : "=a"(_Info[0]), "=b"(_Info[1]), "=c"(_Info[2]), "=d"(_Info[3])
        : "a"(_Type), "c"(_SubType));
}

/* RDTSC */
static __inline unsigned long long __rdtsc(void) {
    unsigned int __lo, __hi;
    __asm__ __volatile__("rdtsc" : "=a"(__lo), "=d"(__hi));
    return ((unsigned long long)__hi << 32) | __lo;
}

/* Interrupt enable/disable */
static __inline void _enable(void) {
    __asm__ __volatile__("sti");
}

static __inline void _disable(void) {
    __asm__ __volatile__("cli");
}

#endif /* _INTRIN_H */

#ifndef RCC_BOOTSTRAP_STDDEF_H
#define RCC_BOOTSTRAP_STDDEF_H

#if !defined(__cplusplus) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 202311L
typedef __rcc_nullptr_t nullptr_t;
#define __STDC_VERSION_STDDEF_H__ 202311L
#endif

#if defined(__x86_64__)
typedef unsigned long long size_t;
typedef long long ptrdiff_t;
typedef long long max_align_t;
#else
typedef unsigned int size_t;
typedef int ptrdiff_t;
typedef long long max_align_t;
#endif

#define NULL ((void*)0)
#define offsetof(type, member) __builtin_offsetof(type, member)

#endif

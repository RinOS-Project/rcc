#ifndef RCC_STDDEF_H
#define RCC_STDDEF_H

#if defined(__x86_64__)
typedef unsigned long long size_t;
typedef long long ptrdiff_t;
#else
typedef unsigned int size_t;
typedef int ptrdiff_t;
#endif

#define NULL ((void*)0)
#define offsetof(type, member) __builtin_offsetof(type, member)

#endif

#ifndef RCC_STDINT_H
#define RCC_STDINT_H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

#if defined(__x86_64__)
typedef long long intptr_t;
typedef unsigned long long uintptr_t;
#define INTPTR_MAX 0x7fffffffffffffff
#define UINTPTR_MAX UINT64_MAX
#else
typedef int intptr_t;
typedef unsigned int uintptr_t;
#define INTPTR_MAX 0x7fffffff
#define UINTPTR_MAX UINT32_MAX
#endif

#define INT32_C(value) value
#define UINT16_C(value) value
#define UINT32_C(value) value
#define UINT64_C(value) value
#define UINT32_MAX 0xffffffff
#define UINT64_MAX 0xffffffffffffffff

#endif

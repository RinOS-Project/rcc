#ifndef RCC_BOOTSTRAP_STDINT_H
#define RCC_BOOTSTRAP_STDINT_H

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
#else
typedef int intptr_t;
typedef unsigned int uintptr_t;
#endif

#define INT8_MIN (-127 - 1)
#define INT8_MAX 127
#define UINT8_MAX 255u
#define INT16_MIN (-32767 - 1)
#define INT16_MAX 32767
#define UINT16_MAX 65535u
#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295u
#define INT64_MIN (-9223372036854775807LL - 1LL)
#define INT64_MAX 9223372036854775807LL
#define UINT64_MAX 18446744073709551615ULL

#define INT8_C(value) value
#define UINT8_C(value) value
#define INT16_C(value) value
#define UINT16_C(value) value
#define INT32_C(value) value
#define UINT32_C(value) value
#define INT64_C(value) ((value) + 0LL)
#define UINT64_C(value) ((value) + 0ULL)

#if defined(__x86_64__)
#define SIZE_MAX UINT64_MAX
#else
#define SIZE_MAX UINT32_MAX
#endif

#endif

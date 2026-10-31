/* SPDX-License-Identifier: MIT */
#define __rin_atomic_load_v1 __rin_atomic_load_v1_native
#define __rin_atomic_store_v1 __rin_atomic_store_v1_native
#define __rin_atomic_exchange_v1 __rin_atomic_exchange_v1_native
#define __rin_atomic_compare_exchange_v1 \
    __rin_atomic_compare_exchange_v1_native
#include "../../../libs/libc/atomic_runtime_fallback.c"
#undef __rin_atomic_load_v1
#undef __rin_atomic_store_v1
#undef __rin_atomic_exchange_v1
#undef __rin_atomic_compare_exchange_v1

#if defined(__GNUC__) && defined(__x86_64__)
#define RINOS_SYSV_ABI __attribute__((sysv_abi))
#else
#define RINOS_SYSV_ABI
#endif

RINOS_SYSV_ABI void __rin_atomic_load_v1(
    const volatile void* object, void* result, size_t size,
    int memory_order)
{
    __rin_atomic_load_v1_native(object, result, size, memory_order);
}

RINOS_SYSV_ABI void __rin_atomic_store_v1(
    volatile void* object, const void* value, size_t size,
    int memory_order)
{
    __rin_atomic_store_v1_native(object, value, size, memory_order);
}

RINOS_SYSV_ABI void __rin_atomic_exchange_v1(
    volatile void* object, const void* desired, void* result,
    size_t size, int memory_order)
{
    __rin_atomic_exchange_v1_native(
        object, desired, result, size, memory_order);
}

RINOS_SYSV_ABI int __rin_atomic_compare_exchange_v1(
    volatile void* object, void* expected, const void* desired,
    size_t size, int weak, int success_order, int failure_order)
{
    return __rin_atomic_compare_exchange_v1_native(
        object, expected, desired, size, weak,
        success_order, failure_order);
}

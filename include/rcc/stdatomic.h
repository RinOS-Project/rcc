#ifndef RCC_STDATOMIC_H
#define RCC_STDATOMIC_H

/* This RinOS C17 surface provides lock-free arithmetic and bitwise operations
 * for 8/16/32/64-bit integer storage on both targets. i686 64-bit operations
 * use the target baseline CMPXCHG8B feature. Pointer load/store/exchange/CAS
 * are lock-free on both targets. */
typedef enum memory_order {
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

#define _Atomic(type) type volatile

typedef volatile _Bool atomic_bool;
typedef volatile char atomic_char;
typedef volatile signed char atomic_schar;
typedef volatile unsigned char atomic_uchar;
typedef volatile short atomic_short;
typedef volatile unsigned short atomic_ushort;
typedef volatile int atomic_int;
typedef volatile unsigned int atomic_uint;
typedef volatile long atomic_long;
typedef volatile unsigned long atomic_ulong;
typedef volatile long long atomic_llong;
typedef volatile unsigned long long atomic_ullong;
typedef volatile unsigned short atomic_char16_t;
typedef volatile unsigned int atomic_char32_t;
typedef volatile int atomic_wchar_t;
typedef volatile signed char atomic_int_least8_t;
typedef volatile unsigned char atomic_uint_least8_t;
typedef volatile short atomic_int_least16_t;
typedef volatile unsigned short atomic_uint_least16_t;
typedef volatile int atomic_int_least32_t;
typedef volatile unsigned int atomic_uint_least32_t;
typedef volatile long long atomic_int_least64_t;
typedef volatile unsigned long long atomic_uint_least64_t;
typedef volatile signed char atomic_int_fast8_t;
typedef volatile unsigned char atomic_uint_fast8_t;
typedef volatile short atomic_int_fast16_t;
typedef volatile unsigned short atomic_uint_fast16_t;
typedef volatile int atomic_int_fast32_t;
typedef volatile unsigned int atomic_uint_fast32_t;
typedef volatile long long atomic_int_fast64_t;
typedef volatile unsigned long long atomic_uint_fast64_t;
#if defined(__x86_64__)
typedef volatile long long atomic_intptr_t;
typedef volatile unsigned long long atomic_uintptr_t;
typedef volatile unsigned long long atomic_size_t;
typedef volatile long long atomic_ptrdiff_t;
#else
typedef volatile int atomic_intptr_t;
typedef volatile unsigned int atomic_uintptr_t;
typedef volatile unsigned int atomic_size_t;
typedef volatile int atomic_ptrdiff_t;
#endif
typedef volatile long long atomic_intmax_t;
typedef volatile unsigned long long atomic_uintmax_t;
typedef volatile unsigned int atomic_flag;

#define ATOMIC_BOOL_LOCK_FREE 2
#define ATOMIC_CHAR_LOCK_FREE 2
#define ATOMIC_CHAR16_T_LOCK_FREE 2
#define ATOMIC_CHAR32_T_LOCK_FREE 2
#define ATOMIC_WCHAR_T_LOCK_FREE 2
#define ATOMIC_SHORT_LOCK_FREE 2
#define ATOMIC_INT_LOCK_FREE 2
#if defined(__i386__)
#define ATOMIC_LONG_LOCK_FREE 2
#define ATOMIC_LLONG_LOCK_FREE 2
#else
#define ATOMIC_LONG_LOCK_FREE 2
#define ATOMIC_LLONG_LOCK_FREE 2
#endif
#define ATOMIC_POINTER_LOCK_FREE 2

#define ATOMIC_FLAG_INIT 0u
#define ATOMIC_VAR_INIT(value) (value)

#define kill_dependency(value) (value)
#define atomic_is_lock_free(object) \
    (sizeof(*(object)) == 1u || sizeof(*(object)) == 2u || \
     sizeof(*(object)) == 4u || sizeof(*(object)) == 8u)
#define atomic_init(object, desired) (*(object) = (desired))

#define atomic_thread_fence(order) __atomic_thread_fence(order)
/* A hardware fence is stronger than the compiler-only signal fence required
 * by C17 and preserves the same observable contract. */
#define atomic_signal_fence(order) __atomic_thread_fence(order)

#define atomic_store_explicit(object, desired, order) \
    __atomic_store_n((object), (desired), (order))
#define atomic_store(object, desired) \
    atomic_store_explicit((object), (desired), memory_order_seq_cst)
#define atomic_load_explicit(object, order) \
    __atomic_load_n((object), (order))
#define atomic_load(object) \
    atomic_load_explicit((object), memory_order_seq_cst)
#define atomic_exchange_explicit(object, desired, order) \
    __atomic_exchange_n((object), (desired), (order))
#define atomic_exchange(object, desired) \
    atomic_exchange_explicit((object), (desired), memory_order_seq_cst)

#define atomic_compare_exchange_strong_explicit( \
        object, expected, desired, success, failure) \
    __atomic_compare_exchange_n((object), (expected), (desired), 0, \
                                (success), (failure))
#define atomic_compare_exchange_weak_explicit( \
        object, expected, desired, success, failure) \
    __atomic_compare_exchange_n((object), (expected), (desired), 1, \
                                (success), (failure))
#define atomic_compare_exchange_strong(object, expected, desired) \
    atomic_compare_exchange_strong_explicit( \
        (object), (expected), (desired), memory_order_seq_cst, \
        memory_order_seq_cst)
#define atomic_compare_exchange_weak(object, expected, desired) \
    atomic_compare_exchange_weak_explicit( \
        (object), (expected), (desired), memory_order_seq_cst, \
        memory_order_seq_cst)

#define atomic_fetch_add_explicit(object, operand, order) \
    __atomic_fetch_add((object), (operand), (order))
#define atomic_fetch_add(object, operand) \
    atomic_fetch_add_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_sub_explicit(object, operand, order) \
    __atomic_fetch_sub((object), (operand), (order))
#define atomic_fetch_sub(object, operand) \
    atomic_fetch_sub_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_and_explicit(object, operand, order) \
    __atomic_fetch_and((object), (operand), (order))
#define atomic_fetch_and(object, operand) \
    atomic_fetch_and_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_or_explicit(object, operand, order) \
    __atomic_fetch_or((object), (operand), (order))
#define atomic_fetch_or(object, operand) \
    atomic_fetch_or_explicit((object), (operand), memory_order_seq_cst)
#define atomic_fetch_xor_explicit(object, operand, order) \
    __atomic_fetch_xor((object), (operand), (order))
#define atomic_fetch_xor(object, operand) \
    atomic_fetch_xor_explicit((object), (operand), memory_order_seq_cst)

#define atomic_flag_test_and_set_explicit(object, order) \
    (__atomic_exchange_n((object), 1u, (order)) != 0u)
#define atomic_flag_test_and_set(object) \
    atomic_flag_test_and_set_explicit((object), memory_order_seq_cst)
#define atomic_flag_clear_explicit(object, order) \
    __atomic_store_n((object), 0u, (order))
#define atomic_flag_clear(object) \
    atomic_flag_clear_explicit((object), memory_order_seq_cst)

#endif

#include <stdint.h>
#include <stdatomic.h>

_Static_assert(ATOMIC_INT_LOCK_FREE == 2,
               "32-bit integer atomics must be lock-free");
_Static_assert(ATOMIC_CHAR_LOCK_FREE == 2,
               "8-bit integer atomics must be lock-free");
_Static_assert(ATOMIC_SHORT_LOCK_FREE == 2,
               "16-bit integer atomics must be lock-free");
#if defined(__x86_64__)
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 2,
               "AMD64 64-bit integer atomics must be lock-free");
#else
_Static_assert(ATOMIC_LLONG_LOCK_FREE == 0,
               "i686 64-bit integer atomics require cmpxchg8b lowering");
#endif
_Static_assert(sizeof(atomic_uint) == 4,
               "atomic_uint must use 32-bit storage");
_Static_assert(sizeof(atomic_char16_t) == 2,
               "atomic_char16_t must use 16-bit storage");
_Static_assert(sizeof(atomic_char32_t) == 4,
               "atomic_char32_t must use 32-bit storage");
_Static_assert(sizeof(atomic_wchar_t) == 4,
               "atomic_wchar_t must use 32-bit storage");
_Static_assert(sizeof(atomic_llong) == 8,
               "atomic_llong must remain available as non-lock-free storage");
_Static_assert(sizeof(atomic_int_least64_t) == 8,
               "least64 atomic storage must remain available");
_Static_assert(sizeof(atomic_uint_fast64_t) == 8,
               "fast64 atomic storage must remain available");
_Static_assert(sizeof(atomic_intmax_t) == 8,
               "intmax atomic storage must remain available");
_Static_assert(sizeof(atomic_uintmax_t) == 8,
               "uintmax atomic storage must remain available");
#if defined(__x86_64__)
_Static_assert(sizeof(atomic_long) == 8 && ATOMIC_LONG_LOCK_FREE == 2,
               "AMD64 long atomics must use the 64-bit lock-free path");
_Static_assert(sizeof(atomic_intptr_t) == 8 && sizeof(atomic_size_t) == 8,
               "AMD64 pointer-sized atomic typedefs must be 64-bit");
#else
_Static_assert(sizeof(atomic_long) == 4 && ATOMIC_LONG_LOCK_FREE == 2,
               "i686 long atomics must use the 32-bit lock-free path");
_Static_assert(sizeof(atomic_intptr_t) == 4 && sizeof(atomic_size_t) == 4,
               "i686 pointer-sized atomic typedefs must be 32-bit");
#endif
_Static_assert(ATOMIC_CHAR16_T_LOCK_FREE == 2 &&
               ATOMIC_CHAR32_T_LOCK_FREE == 2 &&
               ATOMIC_WCHAR_T_LOCK_FREE == 2,
               "fixed-width character atomics must be lock-free");
_Static_assert(ATOMIC_POINTER_LOCK_FREE == 2,
               "pointer atomics must be lock-free on both targets");

void* atomic_pointer_load_value(void* volatile* value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

void atomic_pointer_store_value(void* volatile* value, void* desired) {
    __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

void* atomic_pointer_exchange_value(void* volatile* value, void* desired) {
    return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}

int atomic_pointer_compare_exchange_value(void* volatile* value,
                                          void** expected,
                                          void* desired) {
    return __atomic_compare_exchange_n(value, expected, desired, 0,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
}

void* standard_atomic_pointer_exchange_value(_Atomic(void*)* value,
                                             void* desired) {
    return atomic_exchange(value, desired);
}

#if defined(__i386__)
long standard_atomic_long_fetch_xor_value(atomic_long* value, long operand) {
    return atomic_fetch_xor(value, operand);
}
#endif

#if defined(__x86_64__)
uint64_t atomic_u64_load_value(volatile uint64_t* value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

void atomic_u64_store_value(volatile uint64_t* value, uint64_t desired) {
    __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

uint64_t atomic_u64_exchange_value(volatile uint64_t* value,
                                   uint64_t desired) {
    return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}

uint64_t atomic_u64_fetch_add_value(volatile uint64_t* value,
                                    uint64_t operand) {
    return __atomic_fetch_add(value, operand, __ATOMIC_RELAXED);
}

uint64_t atomic_u64_add_fetch_value(volatile uint64_t* value,
                                    uint64_t operand) {
    return __atomic_add_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint64_t atomic_u64_fetch_sub_value(volatile uint64_t* value,
                                    uint64_t operand) {
    return __atomic_fetch_sub(value, operand, __ATOMIC_RELAXED);
}

uint64_t atomic_u64_sub_fetch_value(volatile uint64_t* value,
                                    uint64_t operand) {
    return __atomic_sub_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint64_t atomic_u64_fetch_xor_value(volatile uint64_t* value,
                                    uint64_t operand) {
    return __atomic_fetch_xor(value, operand, __ATOMIC_ACQ_REL);
}

uint64_t atomic_u64_or_fetch_value(volatile uint64_t* value,
                                   uint64_t operand) {
    return __atomic_or_fetch(value, operand, __ATOMIC_SEQ_CST);
}

int atomic_u64_compare_exchange_value(volatile uint64_t* value,
                                      uint64_t* expected,
                                      uint64_t desired) {
    return __atomic_compare_exchange_n(value, expected, desired, 0,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
}

uint64_t standard_atomic_ullong_fetch_add_value(atomic_ullong* value,
                                                 uint64_t operand) {
    return atomic_fetch_add(value, operand);
}

int standard_atomic_ullong_is_lock_free_value(atomic_ullong* value) {
    return atomic_is_lock_free(value);
}
#endif

uint32_t atomic_load_value(volatile uint32_t* value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

uint32_t atomic_dynamic_load_value(volatile uint32_t* value, int order) {
    return __atomic_load_n(value, order);
}

uint32_t atomic_u8_load_value(volatile uint8_t* value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

void atomic_u8_store_value(volatile uint8_t* value, uint32_t desired) {
    __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

uint32_t atomic_u8_exchange_value(volatile uint8_t* value,
                                  uint32_t desired) {
    return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}

uint32_t atomic_u8_fetch_add_value(volatile uint8_t* value,
                                   uint32_t operand) {
    return __atomic_fetch_add(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_u8_add_fetch_value(volatile uint8_t* value,
                                   uint32_t operand) {
    return __atomic_add_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t atomic_u8_fetch_sub_value(volatile uint8_t* value,
                                   uint32_t operand) {
    return __atomic_fetch_sub(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_u8_sub_fetch_value(volatile uint8_t* value,
                                   uint32_t operand) {
    return __atomic_sub_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t atomic_u8_fetch_nand_value(volatile uint8_t* value,
                                    uint32_t operand) {
    return __atomic_fetch_nand(value, operand, __ATOMIC_ACQ_REL);
}

int atomic_u8_compare_exchange_value(volatile uint8_t* value,
                                     uint8_t* expected,
                                     uint32_t desired) {
    return __atomic_compare_exchange_n(value, expected, desired, 0,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
}

uint32_t atomic_u16_load_value(volatile uint16_t* value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

void atomic_u16_store_value(volatile uint16_t* value, uint32_t desired) {
    __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

uint32_t atomic_u16_exchange_value(volatile uint16_t* value,
                                   uint32_t desired) {
    return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}

uint32_t atomic_u16_fetch_add_value(volatile uint16_t* value,
                                    uint32_t operand) {
    return __atomic_fetch_add(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_u16_add_fetch_value(volatile uint16_t* value,
                                    uint32_t operand) {
    return __atomic_add_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t atomic_u16_fetch_sub_value(volatile uint16_t* value,
                                    uint32_t operand) {
    return __atomic_fetch_sub(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_u16_sub_fetch_value(volatile uint16_t* value,
                                    uint32_t operand) {
    return __atomic_sub_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t atomic_u16_xor_fetch_value(volatile uint16_t* value,
                                    uint32_t operand) {
    return __atomic_xor_fetch(value, operand, __ATOMIC_ACQ_REL);
}

int atomic_u16_compare_exchange_value(volatile uint16_t* value,
                                      uint16_t* expected,
                                      uint32_t desired) {
    return __atomic_compare_exchange_n(value, expected, desired, 0,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
}

int32_t atomic_i8_fetch_add_value(volatile int8_t* value,
                                  int32_t operand) {
    return __atomic_fetch_add(value, operand, __ATOMIC_SEQ_CST);
}

int32_t atomic_i16_fetch_sub_value(volatile int16_t* value,
                                   int32_t operand) {
    return __atomic_fetch_sub(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t sync_u8_compare_exchange_value(volatile uint8_t* value,
                                        uint32_t expected,
                                        uint32_t desired) {
    return __sync_val_compare_and_swap(value, expected, desired);
}

void sync_u8_release_value(volatile uint8_t* value) {
    __sync_lock_release(value);
}

void atomic_store_value(volatile uint32_t* value, uint32_t desired) {
    __atomic_store_n(value, desired, __ATOMIC_RELEASE);
}

uint32_t atomic_exchange_value(volatile uint32_t* value, uint32_t desired) {
    return __atomic_exchange_n(value, desired, __ATOMIC_ACQ_REL);
}

uint32_t atomic_fetch_add_value(volatile uint32_t* value, uint32_t operand) {
    return __atomic_fetch_add(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_fetch_sub_value(volatile uint32_t* value, uint32_t operand) {
    return __atomic_fetch_sub(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_add_fetch_value(volatile uint32_t* value, uint32_t operand) {
    return __atomic_add_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t atomic_sub_fetch_value(volatile uint32_t* value, uint32_t operand) {
    return __atomic_sub_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t atomic_fetch_and_value(volatile uint32_t* value,
                                uint32_t operand) {
    return __atomic_fetch_and(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_and_fetch_value(volatile uint32_t* value,
                                uint32_t operand) {
    return __atomic_and_fetch(value, operand, __ATOMIC_ACQUIRE);
}

uint32_t atomic_fetch_or_value(volatile uint32_t* value,
                               uint32_t operand) {
    return __atomic_fetch_or(value, operand, __ATOMIC_RELEASE);
}

uint32_t atomic_or_fetch_value(volatile uint32_t* value,
                               uint32_t operand) {
    return __atomic_or_fetch(value, operand, __ATOMIC_ACQ_REL);
}

uint32_t atomic_fetch_xor_value(volatile uint32_t* value,
                                uint32_t operand) {
    return __atomic_fetch_xor(value, operand, __ATOMIC_SEQ_CST);
}

uint32_t atomic_xor_fetch_value(volatile uint32_t* value,
                                uint32_t operand) {
    return __atomic_xor_fetch(value, operand, __ATOMIC_RELAXED);
}

uint32_t atomic_fetch_nand_value(volatile uint32_t* value,
                                 uint32_t operand) {
    return __atomic_fetch_nand(value, operand, __ATOMIC_ACQUIRE);
}

uint32_t atomic_nand_fetch_value(volatile uint32_t* value,
                                 uint32_t operand) {
    return __atomic_nand_fetch(value, operand, __ATOMIC_SEQ_CST);
}

uint64_t atomic_nand_fetch_widened_value(volatile uint32_t* value,
                                         uint32_t operand) {
    return __atomic_nand_fetch(value, operand, __ATOMIC_SEQ_CST);
}

int64_t atomic_i32_xor_fetch_widened_value(volatile int32_t* value,
                                           int32_t operand) {
    return __atomic_xor_fetch(value, operand, __ATOMIC_SEQ_CST);
}

int atomic_compare_exchange_bool(volatile uint32_t* value,
                                 uint32_t expected,
                                 uint32_t desired) {
    return __sync_bool_compare_and_swap(value, expected, desired);
}

uint32_t atomic_compare_exchange_value(volatile uint32_t* value,
                                       uint32_t expected,
                                       uint32_t desired) {
    return __sync_val_compare_and_swap(value, expected, desired);
}

uint32_t sync_exchange_value(volatile uint32_t* value, uint32_t desired) {
    return __sync_lock_test_and_set(value, desired);
}

void sync_release_value(volatile uint32_t* value) {
    __sync_lock_release(value);
}

uint32_t sync_fetch_add_value(volatile uint32_t* value, uint32_t operand) {
    return __sync_fetch_and_add(value, operand);
}

uint32_t sync_fetch_sub_value(volatile uint32_t* value, uint32_t operand) {
    return __sync_fetch_and_sub(value, operand);
}

uint32_t sync_add_fetch_value(volatile uint32_t* value, uint32_t operand) {
    return __sync_add_and_fetch(value, operand);
}

uint32_t sync_sub_fetch_value(volatile uint32_t* value, uint32_t operand) {
    return __sync_sub_and_fetch(value, operand);
}

uint32_t sync_fetch_or_value(volatile uint32_t* value, uint32_t operand) {
    return __sync_fetch_and_or(value, operand);
}

uint32_t sync_xor_fetch_value(volatile uint32_t* value, uint32_t operand) {
    return __sync_xor_and_fetch(value, operand);
}

void atomic_thread_fence_value(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void sync_synchronize_value(void) {
    __sync_synchronize();
}

void standard_atomic_init_value(atomic_uint* value, uint32_t desired) {
    atomic_init(value, desired);
}

uint32_t standard_atomic_load_value(atomic_uint* value) {
    return atomic_load_explicit(value, memory_order_acquire);
}

void standard_atomic_store_value(atomic_uint* value, uint32_t desired) {
    atomic_store_explicit(value, desired, memory_order_release);
}

uint32_t standard_atomic_exchange_value(atomic_uint* value,
                                        uint32_t desired) {
    return atomic_exchange(value, desired);
}

uint32_t standard_atomic_fetch_add_value(atomic_uint* value,
                                         uint32_t operand) {
    return atomic_fetch_add(value, operand);
}

uint32_t standard_atomic_fetch_and_value(atomic_uint* value,
                                         uint32_t operand) {
    return atomic_fetch_and_explicit(value, operand, memory_order_relaxed);
}

uint32_t standard_atomic_fetch_or_value(atomic_uint* value,
                                        uint32_t operand) {
    return atomic_fetch_or(value, operand);
}

uint32_t standard_atomic_fetch_xor_value(atomic_uint* value,
                                         uint32_t operand) {
    return atomic_fetch_xor_explicit(value, operand, memory_order_acq_rel);
}

int standard_atomic_compare_exchange_value(atomic_uint* value,
                                           uint32_t* expected,
                                           uint32_t desired) {
    return atomic_compare_exchange_strong_explicit(value, expected, desired, \
        memory_order_acq_rel, memory_order_acquire);
}

int standard_atomic_flag_test_and_set_value(atomic_flag* value) {
    return atomic_flag_test_and_set(value);
}

void standard_atomic_flag_clear_value(atomic_flag* value) {
    atomic_flag_clear_explicit(value, memory_order_release);
}

int standard_atomic_is_lock_free_value(atomic_uint* value) {
    return atomic_is_lock_free(value);
}

void standard_atomic_signal_fence_value(void) {
    atomic_signal_fence(memory_order_seq_cst);
}

uint32_t standard_atomic_uchar_fetch_add_value(atomic_uchar* value,
                                                uint32_t operand) {
    return atomic_fetch_add(value, operand);
}

uint32_t standard_atomic_ushort_exchange_value(atomic_ushort* value,
                                                uint32_t desired) {
    return atomic_exchange(value, desired);
}

int standard_atomic_bool_exchange_value(atomic_bool* value, int desired) {
    return atomic_exchange(value, desired);
}

int main(void) {
    return 0;
}

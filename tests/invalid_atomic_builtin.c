#include <stdint.h>

uint64_t unsupported_wide_atomic(volatile uint64_t* value) {
    return __atomic_fetch_add(value, 1u, __ATOMIC_RELAXED);
}

int unsupported_wide_expected(volatile uint32_t* value,
                              uint64_t* expected) {
    return __atomic_compare_exchange_n(value, expected, 1u, 0,
                                       __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
}

int mismatched_signed_expected(volatile uint32_t* value,
                               int32_t* expected) {
    return __atomic_compare_exchange_n(value, expected, 1u, 0,
                                       __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
}

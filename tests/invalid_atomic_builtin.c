#include <stdint.h>

struct generic_atomic_aggregate {
    uint32_t value;
};

void invalid_generic_aggregate(struct generic_atomic_aggregate* value,
                               struct generic_atomic_aggregate* result) {
    __atomic_load(value, result, __ATOMIC_ACQUIRE);
}

int invalid_always_lock_free_nonconstant(unsigned size) {
    return __atomic_always_lock_free(size, (void*)0);
}

int invalid_lock_free_size_type(void) {
    return __atomic_is_lock_free("size", (void*)0);
}

int invalid_lock_free_pointer_type(unsigned size, int value) {
    return __atomic_is_lock_free(size, value);
}

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

int invalid_test_and_set_width(volatile uint32_t* value) {
    return __atomic_test_and_set(value, __ATOMIC_SEQ_CST);
}

void invalid_clear_width(volatile uint32_t* value) {
    __atomic_clear(value, __ATOMIC_SEQ_CST);
}

#include <stdint.h>

uint32_t invalid_atomic_load_order(volatile uint32_t* value) {
    return __atomic_load_n(value, __ATOMIC_RELEASE);
}

void invalid_atomic_store_order(volatile uint32_t* value) {
    __atomic_store_n(value, 1u, __ATOMIC_ACQUIRE);
}

uint32_t invalid_atomic_order_range(volatile uint32_t* value) {
    return __atomic_exchange_n(value, 1u, __ATOMIC_ACQUIRE + 4);
}

uint32_t invalid_negative_atomic_order(volatile uint32_t* value) {
    return __atomic_load_n(value, -1);
}

uint32_t invalid_atomic_order_type(volatile uint32_t* value,
                                   double order) {
    return __atomic_load_n(value, order);
}

int invalid_atomic_failure_release(volatile uint32_t* value,
                                   uint32_t* expected) {
    return __atomic_compare_exchange_n(value, expected, 1u, 0,
                                       __ATOMIC_SEQ_CST,
                                       __ATOMIC_RELEASE);
}

int invalid_atomic_failure_stronger(volatile uint32_t* value,
                                    uint32_t* expected) {
    return __atomic_compare_exchange_n(value, expected, 1u, 0,
                                       __ATOMIC_RELAXED,
                                       __ATOMIC_ACQUIRE);
}

int invalid_atomic_weak_flag(volatile uint32_t* value,
                             uint32_t* expected) {
    return __atomic_compare_exchange_n(value, expected, 1u, 2,
                                       __ATOMIC_SEQ_CST,
                                       __ATOMIC_SEQ_CST);
}

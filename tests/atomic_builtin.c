#include <stdint.h>

uint32_t atomic_load_value(volatile uint32_t* value) {
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
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

void atomic_thread_fence_value(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}

void sync_synchronize_value(void) {
    __sync_synchronize();
}

int main(void) {
    return 0;
}

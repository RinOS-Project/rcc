#include <stdint.h>

static volatile uint32_t rcc_atomic_state;

int rcc_atomic_compare_exchange(uint32_t expected, uint32_t desired) {
    return __sync_bool_compare_and_swap(
        &rcc_atomic_state, expected, desired);
}

void rcc_atomic_store(uint32_t value) {
    __atomic_store_n(&rcc_atomic_state, value, __ATOMIC_RELEASE);
}

uint32_t rcc_atomic_load(void) {
    return __atomic_load_n(&rcc_atomic_state, __ATOMIC_ACQUIRE);
}

int main(void) {
    rcc_atomic_store(1u);
    return rcc_atomic_compare_exchange(1u, 2u) && rcc_atomic_load() == 2u
        ? 0 : 1;
}

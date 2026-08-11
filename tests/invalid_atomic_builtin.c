#include <stdint.h>

uint64_t unsupported_wide_atomic(volatile uint64_t* value) {
    return __atomic_fetch_add(value, 1u, __ATOMIC_RELAXED);
}

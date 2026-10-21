#include <atomic>

extern "C" int cpp_atomic_always_lock_free_value() {
    return __atomic_always_lock_free(8, nullptr) &&
           __atomic_is_lock_free(4, nullptr);
}

extern "C" int cpp_atomic_is_lock_free_value(unsigned count, int* cursor) {
    return __atomic_is_lock_free(count, cursor++);
}

extern "C" int cpp_signed_parameter_value(signed value) {
    return value;
}

extern "C" void cpp_atomic_generic_load(const unsigned* value,
                                          unsigned* result) {
    __atomic_load(value, result, __ATOMIC_ACQUIRE);
}

extern "C" void cpp_atomic_generic_store(unsigned* value,
                                           unsigned* desired) {
    __atomic_store(value, desired, __ATOMIC_RELEASE);
}

extern "C" unsigned cpp_atomic_generic_exchange(unsigned* value,
                                                  unsigned* desired) {
    unsigned previous;
    __atomic_exchange(value, desired, &previous, __ATOMIC_ACQ_REL);
    return previous;
}

extern "C" bool cpp_atomic_generic_compare(unsigned* value,
                                            unsigned* expected,
                                            unsigned* desired) {
    return __atomic_compare_exchange(value, expected, desired, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

extern "C" unsigned cpp_standard_atomic_operations() {
    std::atomic<unsigned> zero;
    std::atomic<unsigned> value(3u);
    std::atomic<unsigned long long> wide(4ULL);
    unsigned expected = 9u;
    if (zero.load() != 0u || !zero.is_lock_free()) return 20u;
    if (wide.fetch_add(3ULL) != 4ULL || wide.load() != 7ULL) return 21u;
    if (value.load(std::memory_order_relaxed) != 3u) return 1u;
    value.store(7u, std::memory_order_release);
    if (value.exchange(9u, std::memory_order_acq_rel) != 7u) return 2u;
    if (!value.compare_exchange_strong(
            expected, 13u, std::memory_order_acq_rel,
            std::memory_order_acquire)) return 3u;
    if (value.fetch_add(2u, std::memory_order_relaxed) != 13u) return 4u;
    if (value.fetch_and(7u, std::memory_order_seq_cst) != 15u) return 5u;
    if (value.fetch_or(8u) != 7u) return 6u;
    if (value.fetch_xor(3u) != 15u) return 7u;
    if (value++ != 12u || value.load() != 13u) return 8u;
    if (--value != 12u || value.load() != 12u) return 9u;
    expected = 0u;
    if (value.compare_exchange_strong(expected, 99u) || expected != 12u)
        return 10u;
    expected = 12u;
    if (!value.compare_exchange_weak(
            expected, 14u, std::memory_order_release)) return 12u;
    value.store(12u);
    if ((value = 5u) != 5u || value.load() != 5u) return 11u;
    value.store(12u);
    return value.load();
}

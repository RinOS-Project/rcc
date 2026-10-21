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
    int pointer_values[5];
    std::atomic<int*> pointer;
    std::atomic<int*> initialized_pointer(pointer_values + 1);
    std::atomic<int*>::difference_type offset = 2;
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    int* pointer_expected;
    std::atomic<float> floating;
    float floating_expected = 9.0f;
    unsigned expected = 9u;
    if (zero.load() != 0u || !zero.is_lock_free()) return 20u;
    if (wide.fetch_add(3ULL) != 4ULL || wide.load() != 7ULL) return 21u;
    if (initialized_pointer.load() != pointer_values + 1) return 29u;
    pointer.store(pointer_values);
    if (flag.test(std::memory_order_relaxed)) return 30u;
    if (flag.test_and_set(std::memory_order_acquire)) return 31u;
    if (!flag.test()) return 32u;
    if (!flag.test_and_set()) return 33u;
    flag.clear(std::memory_order_release);
    if (flag.test()) return 34u;
    if (flag.test_and_set()) return 35u;
    flag.clear();
    if (floating.load() != 0.0f) return 36u;
    floating.store(1.5f);
    if (floating.load(std::memory_order_relaxed) != 1.5f) return 48u;
    if (floating.fetch_add(2.0f, std::memory_order_acq_rel) != 1.5f ||
        floating.load() != 3.5f) return 37u;
    if (floating.fetch_sub(0.5f) != 3.5f || floating.load() != 3.0f)
        return 38u;
    if (floating.compare_exchange_strong(
            floating_expected, 6.0f, std::memory_order_acq_rel,
            std::memory_order_acquire) || floating_expected != 3.0f)
        return 39u;
    floating_expected = 3.0f;
    if (!floating.compare_exchange_weak(
            floating_expected, 4.0f, std::memory_order_release) ||
        floating.load() != 4.0f) return 40u;
    if (floating.exchange(5.5f, std::memory_order_relaxed) != 4.0f ||
        floating.load() != 5.5f) return 41u;
    if ((floating = 7.0f) != 7.0f || floating.load() != 7.0f) return 42u;
#if defined(__x86_64__)
    {
        std::atomic<double> floating_wide;
        double floating_wide_expected = 3.0;
        if (floating_wide.load() != 0.0) return 47u;
        floating_wide.store(2.25);
        if (floating_wide.fetch_add(1.5) != 2.25 ||
            floating_wide.load() != 3.75) return 43u;
        if (floating_wide.fetch_sub(0.75, std::memory_order_relaxed) !=
                3.75 || floating_wide.load() != 3.0) return 44u;
        if (!floating_wide.compare_exchange_strong(
                floating_wide_expected, 4.5, std::memory_order_acq_rel,
                std::memory_order_acquire) || floating_wide.load() != 4.5)
            return 45u;
        if ((floating_wide = 6.25) != 6.25 || floating_wide.load() != 6.25)
            return 46u;
    }
#endif
    if (value.load(std::memory_order_relaxed) != 3u) return 1u;
    value.store(7u, std::memory_order_release);
    if (value.exchange(9u, std::memory_order_acq_rel) != 7u) return 2u;
    if (!value.compare_exchange_strong(
            expected, 13u, std::memory_order_acq_rel,
            std::memory_order_acquire)) return 3u;
    if (value.fetch_add(2u, std::memory_order_relaxed) != 13u) return 4u;
    if (value.fetch_and(7u) != 15u) return 5u;
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
    if (pointer.fetch_add(offset) != pointer_values ||
        pointer.load() != pointer_values + 2) return 22u;
    if (pointer++ != pointer_values + 2 ||
        pointer.load() != pointer_values + 3) return 23u;
    if (pointer.fetch_sub(2, std::memory_order_relaxed) !=
            pointer_values + 3 || pointer.load() != pointer_values + 1)
        return 24u;
    if ((pointer += 2) != pointer_values + 3 ||
        pointer.load() != pointer_values + 3) return 25u;
    if (--pointer != pointer_values + 2 ||
        pointer.load() != pointer_values + 2) return 26u;
    pointer_expected = pointer_values + 2;
    if (!pointer.compare_exchange_strong(
            pointer_expected, pointer_values + 4) ||
        pointer.load() != pointer_values + 4) return 27u;
    pointer_expected = pointer_values;
    if (pointer.compare_exchange_strong(
            pointer_expected, pointer_values) ||
        pointer_expected != pointer_values + 4) return 28u;
    return value.load();
}

#include <atomic>

template<class T>
class InheritedStaticTemplateBase {
public:
    using value_type = T;
    static constexpr bool is_always_lock_free =
        sizeof(T) == 1 || sizeof(T) == 2 ||
        sizeof(T) == 4 || sizeof(T) == 8;

    constexpr InheritedStaticTemplateBase() noexcept : value_(0) {}
    constexpr InheritedStaticTemplateBase(T value) noexcept : value_(value) {}
    constexpr T value() const noexcept { return value_; }
    static constexpr T doubled(T value) noexcept { return value * 2; }
    static constexpr T doubled(T value, T multiplier) noexcept {
        return value * multiplier;
    }

private:
    T value_;
};

template<class T>
class InheritedStaticTemplateDerived :
    public InheritedStaticTemplateBase<T> {
public:
    constexpr InheritedStaticTemplateDerived() noexcept
        : InheritedStaticTemplateBase<T>() {}
    constexpr InheritedStaticTemplateDerived(T value) noexcept
        : InheritedStaticTemplateBase<T>(value) {}
};

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
    InheritedStaticTemplateDerived<unsigned>::value_type inherited_value = 3u;
    InheritedStaticTemplateDerived<unsigned> inherited(inherited_value);
    int* pointer_expected;
    std::atomic<float> floating;
    std::atomic<unsigned>::value_type atomic_value_type = 1u;
    float floating_expected = 9.0f;
    unsigned expected = 9u;
    if (!zero.is_always_lock_free) return 80u;
    if (!pointer.is_always_lock_free) return 81u;
    if (!flag.is_always_lock_free) return 82u;
    if (!InheritedStaticTemplateDerived<unsigned>::is_always_lock_free ||
        !inherited.is_always_lock_free || inherited.value() != 3u ||
        InheritedStaticTemplateDerived<unsigned>::doubled(3u) != 6u ||
        InheritedStaticTemplateDerived<unsigned>::doubled(3u, 2u) != 6u ||
        inherited.doubled(3u) != 6u || inherited.doubled(3u, 2u) != 6u)
        return 83u;
    if (zero.load() != 0u || !zero.is_always_lock_free ||
        !zero.is_lock_free()) return 20u;
    if (atomic_value_type != 1u) return 79u;
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
    {
        std::atomic<double> floating_wide;
        double floating_wide_expected = 3.0;
        if (!floating_wide.is_always_lock_free ||
            !floating_wide.is_lock_free()) return 77u;
        if (floating_wide.load() != 0.0) return 47u;
        floating_wide.store(2.25);
        if (floating_wide.fetch_add(1.5) != 2.25 ||
            floating_wide.load() != 3.75) return 43u;
        if (floating_wide.fetch_sub(0.75, std::memory_order_relaxed) !=
                3.75 || floating_wide.load() != 3.0) return 44u;
        if (floating_wide.exchange(4.0, std::memory_order_acq_rel) != 3.0 ||
            floating_wide.load() != 4.0) return 49u;
        floating_wide_expected = 4.0;
        if (!floating_wide.compare_exchange_strong(
                floating_wide_expected, 4.5, std::memory_order_acq_rel,
                std::memory_order_acquire) || floating_wide.load() != 4.5)
            return 45u;
        if ((floating_wide = 6.25) != 6.25 || floating_wide.load() != 6.25)
            return 46u;
    }
    {
        volatile std::atomic<unsigned> volatile_value(2u);
        unsigned volatile_expected = 100u;
        if (!volatile_value.is_lock_free()) return 50u;
        if (volatile_value.load() != 2u) return 75u;
        unsigned volatile_converted = volatile_value;
        if (volatile_converted != 2u) return volatile_converted;
        volatile_value.store(5u, std::memory_order_release);
        if (volatile_value.exchange(7u, std::memory_order_acq_rel) != 5u)
            return 51u;
        if (volatile_value.compare_exchange_strong(
                volatile_expected, 9u, std::memory_order_acq_rel,
                std::memory_order_acquire) || volatile_expected != 7u)
            return 52u;
        volatile_expected = 7u;
        if (!volatile_value.compare_exchange_weak(
                volatile_expected, 8u, std::memory_order_release) ||
            volatile_value.load() != 8u) return 53u;
        if (volatile_value.fetch_add(2u) != 8u ||
            volatile_value.fetch_sub(1u, std::memory_order_relaxed) != 10u)
            return 54u;
        if (volatile_value.fetch_and(7u) != 9u ||
            volatile_value.fetch_or(8u, std::memory_order_relaxed) != 1u ||
            volatile_value.fetch_xor(3u) != 9u) return 55u;
        if (++volatile_value != 11u || volatile_value++ != 11u ||
            --volatile_value != 11u || volatile_value-- != 11u)
            return 56u;
        if ((volatile_value += 4u) != 14u ||
            (volatile_value -= 3u) != 11u ||
            (volatile_value = 12u) != 12u)
            return 57u;
        volatile_value.store(12u);
        volatile_expected = 12u;
        if (!volatile_value.compare_exchange_strong(
                volatile_expected, 13u)) return 67u;
        volatile_expected = 13u;
        if (!volatile_value.compare_exchange_weak(
                volatile_expected, 14u)) return 72u;
        volatile_expected = 14u;
        if (!volatile_value.compare_exchange_strong(
                volatile_expected, 15u, std::memory_order_seq_cst) ||
            volatile_value.load() != 15u) return 73u;
    }
    {
        int volatile_pointer_values[5];
        volatile std::atomic<int*> volatile_pointer(
            volatile_pointer_values);
        int* volatile_pointer_expected = volatile_pointer_values + 2;
        if (volatile_pointer.load() != volatile_pointer_values ||
            volatile_pointer.fetch_add(2) != volatile_pointer_values ||
            volatile_pointer.fetch_sub(1, std::memory_order_relaxed) !=
                volatile_pointer_values + 2)
            return 58u;
        if (volatile_pointer.compare_exchange_strong(
                volatile_pointer_expected, volatile_pointer_values + 3) ||
            volatile_pointer_expected != volatile_pointer_values + 1)
            return 59u;
        volatile_pointer_expected = volatile_pointer_values + 1;
        if (!volatile_pointer.compare_exchange_weak(
                volatile_pointer_expected, volatile_pointer_values + 2,
                std::memory_order_acq_rel) ||
            volatile_pointer++ != volatile_pointer_values + 2 ||
            volatile_pointer.load() != volatile_pointer_values + 3)
            return 60u;
        if ((volatile_pointer -= 2) != volatile_pointer_values + 1 ||
            (volatile_pointer = volatile_pointer_values + 4) !=
                volatile_pointer_values + 4 ||
            volatile_pointer.exchange(volatile_pointer_values) !=
                volatile_pointer_values + 4)
            return 61u;
        volatile_pointer.store(volatile_pointer_values + 1);
        volatile_pointer_expected = volatile_pointer_values + 1;
        if (!volatile_pointer.compare_exchange_strong(
                volatile_pointer_expected, volatile_pointer_values + 2,
                std::memory_order_seq_cst))
            return 68u;
        volatile_pointer_expected = volatile_pointer_values + 2;
        if (!volatile_pointer.compare_exchange_weak(
                volatile_pointer_expected, volatile_pointer_values + 3) ||
            volatile_pointer.load() != volatile_pointer_values + 3)
            return 74u;
    }
    {
        volatile std::atomic<float> volatile_floating(1.0f);
        float volatile_float_expected = 2.0f;
        if (volatile_floating.fetch_add(1.5f) != 1.0f ||
            volatile_floating.fetch_sub(0.5f,
                std::memory_order_relaxed) != 2.5f)
            return 62u;
        if (!volatile_floating.compare_exchange_strong(
                volatile_float_expected, 4.0f,
                std::memory_order_acq_rel,
                std::memory_order_acquire) ||
            volatile_floating.exchange(5.0f) != 4.0f ||
            (volatile_floating = 6.0f) != 6.0f ||
            volatile_floating.load() != 6.0f)
            return 63u;
        volatile_floating.store(7.0f, std::memory_order_release);
        if (volatile_floating.load(std::memory_order_acquire) != 7.0f)
            return 69u;
    }
    {
        volatile std::atomic<double> volatile_floating_wide(2.0);
        double volatile_double_expected = 2.0;
        if (!volatile_floating_wide.is_always_lock_free ||
            !volatile_floating_wide.is_lock_free()) return 78u;
        if (!volatile_floating_wide.compare_exchange_weak(
                volatile_double_expected, 3.0,
                std::memory_order_acq_rel,
                std::memory_order_acquire) ||
            volatile_floating_wide.fetch_add(0.5) != 3.0 ||
            volatile_floating_wide.fetch_sub(0.25) != 3.5 ||
            (volatile_floating_wide = 4.0) != 4.0 ||
            volatile_floating_wide.load() != 4.0)
            return 64u;
        if (volatile_floating_wide.exchange(4.5) != 4.0 ||
            volatile_floating_wide.load() != 4.5) return 76u;
        volatile_floating_wide.store(5.0);
        if (volatile_floating_wide.load() != 5.0) return 70u;
    }
    {
        volatile std::atomic_flag volatile_flag;
        if (volatile_flag.test() || volatile_flag.test_and_set() ||
            !volatile_flag.test(std::memory_order_relaxed))
            return 65u;
        volatile_flag.clear(std::memory_order_release);
        if (volatile_flag.test_and_set(std::memory_order_acquire))
            return 66u;
        volatile_flag.clear();
    }
    {
        const volatile std::atomic<unsigned> const_volatile_value(17u);
        const volatile std::atomic_flag const_volatile_flag;
        if (!const_volatile_value.is_lock_free() ||
            const_volatile_value.load(std::memory_order_acquire) != 17u ||
            const_volatile_flag.test())
            return 71u;
    }
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

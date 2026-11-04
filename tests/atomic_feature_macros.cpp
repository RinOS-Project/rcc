#include <atomic>

template<class Left, class Right>
struct atomic_feature_is_same {
    static constexpr bool value = false;
};

template<class Type>
struct atomic_feature_is_same<Type, Type> {
    static constexpr bool value = true;
};

struct atomic_feature_storage {
    unsigned value;
};

static_assert(sizeof(std::atomic<int>) == sizeof(int));
static_assert(alignof(std::atomic<int>) == alignof(int));
static_assert(sizeof(std::atomic<float>) == sizeof(float));
static_assert(alignof(std::atomic<float>) == alignof(float));
static_assert(sizeof(std::atomic<atomic_feature_storage>) ==
              sizeof(atomic_feature_storage));
static_assert(alignof(std::atomic<atomic_feature_storage>) ==
              alignof(atomic_feature_storage));

#if __cplusplus >= 202002L
template<class Atomic>
struct atomic_feature_rmw_requirements {
    static constexpr bool has_fetch_arithmetic = requires(Atomic& value) {
        value.fetch_add(1);
        value.fetch_sub(1);
    };
    static constexpr bool has_compound_add = requires(Atomic& value) {
        value += 1;
    };
    static constexpr bool has_bitwise_fetch = requires(Atomic& value) {
        value.fetch_and(1);
        value.fetch_or(1);
        value.fetch_xor(1);
    };
    static constexpr bool has_increment = requires(Atomic& value) {
        ++value;
        value--;
    };
};

#if !defined(__cpp_lib_atomic_is_always_lock_free) || \
    __cpp_lib_atomic_is_always_lock_free != 201603L
#error "C++20 atomic is_always_lock_free feature macro mismatch"
#endif
#if !defined(__cpp_lib_atomic_flag_test) || \
    __cpp_lib_atomic_flag_test != 201907L
#error "C++20 atomic_flag::test feature macro mismatch"
#endif
#if !defined(__cpp_lib_atomic_ref) || __cpp_lib_atomic_ref != 201806L
#error "C++20 atomic_ref feature macro mismatch"
#endif
#if !defined(__cpp_lib_atomic_value_initialization) || \
    __cpp_lib_atomic_value_initialization != 201911L
#error "C++20 atomic value initialization feature macro mismatch"
#endif
#if !defined(__cpp_lib_atomic_wait) || __cpp_lib_atomic_wait != 201907L
#error "C++20 atomic wait feature macro mismatch"
#endif
#if !defined(__cpp_lib_atomic_float) || __cpp_lib_atomic_float != 201711L
#error "C++20 floating atomics feature macro mismatch"
#endif
#if !defined(__cpp_lib_atomic_lock_free_type_aliases) || \
    __cpp_lib_atomic_lock_free_type_aliases != 201907L
#error "C++20 lock-free atomic alias feature macro mismatch"
#endif
static_assert(atomic_feature_is_same<std::atomic_signed_lock_free,
                                     std::atomic<signed int>>::value);
static_assert(atomic_feature_is_same<std::atomic_unsigned_lock_free,
                                     std::atomic<unsigned int>>::value);
static_assert(std::atomic<signed int>::is_always_lock_free);
static_assert(std::atomic<unsigned int>::is_always_lock_free);
static_assert(atomic_feature_rmw_requirements<
              std::atomic<float>>::has_fetch_arithmetic);
static_assert(atomic_feature_rmw_requirements<
              std::atomic<double>>::has_fetch_arithmetic);
static_assert(atomic_feature_rmw_requirements<
              std::atomic<float>>::has_compound_add);
static_assert(!atomic_feature_rmw_requirements<
              std::atomic<float>>::has_bitwise_fetch);
static_assert(!atomic_feature_rmw_requirements<
              std::atomic<float>>::has_increment);
static_assert(atomic_feature_rmw_requirements<
              std::atomic<signed int>>::has_bitwise_fetch);
static_assert(atomic_feature_rmw_requirements<
              std::atomic<signed int>>::has_increment);
static_assert(!atomic_feature_rmw_requirements<
              std::atomic<bool>>::has_fetch_arithmetic);
static_assert(atomic_feature_rmw_requirements<
              std::atomic_ref<float>>::has_fetch_arithmetic);
static_assert(atomic_feature_rmw_requirements<
              std::atomic_ref<float>>::has_compound_add);
static_assert(!atomic_feature_rmw_requirements<
              std::atomic_ref<float>>::has_bitwise_fetch);
static_assert(!atomic_feature_rmw_requirements<
              std::atomic_ref<float>>::has_increment);
static_assert(atomic_feature_rmw_requirements<
              std::atomic_ref<signed int>>::has_bitwise_fetch);
static_assert(atomic_feature_rmw_requirements<
              std::atomic_ref<signed int>>::has_increment);
static_assert(requires(std::atomic_flag& flag) { flag.test(); });
static_assert(requires(std::atomic_flag& flag) {
    flag.test(std::memory_order_relaxed);
});
static_assert(requires(std::atomic_flag& flag) {
    std::atomic_flag_test(&flag);
});
static_assert(requires(std::atomic_flag& flag) {
    std::atomic_flag_test_explicit(&flag, std::memory_order_relaxed);
});
#else
#if !defined(__cpp_lib_atomic_is_always_lock_free) || \
    __cpp_lib_atomic_is_always_lock_free != 201603L
#error "C++17 atomic is_always_lock_free feature macro mismatch"
#endif
#if defined(__cpp_lib_atomic_flag_test) || defined(__cpp_lib_atomic_ref) || \
    defined(__cpp_lib_atomic_value_initialization) || \
    defined(__cpp_lib_atomic_wait) || defined(__cpp_lib_atomic_float) || \
    defined(__cpp_lib_atomic_lock_free_type_aliases)
#error "C++20 atomic feature macro advertised in an earlier language mode"
#endif
#endif

int atomic_feature_macro_probe() {
    std::atomic<unsigned> value(1u);
    std::atomic<int> integer_rmw(1);
    std::atomic<void*> opaque_pointer(nullptr);
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
#if __cplusplus >= 202002L
    if (flag.test() || std::atomic_flag_test(&flag)) return 3;
    if (flag.test(std::memory_order_relaxed) ||
        std::atomic_flag_test_explicit(&flag, std::memory_order_relaxed))
        return 4;
    std::atomic_signed_lock_free signed_value(1);
    std::atomic_unsigned_lock_free unsigned_value(1u);
    std::atomic<float> float_value(1.0f);
    std::atomic<double> double_value(1.0);
    signed_value.fetch_add(1);
    unsigned_value.fetch_add(1u);
    float_value.fetch_add(1.0f);
    float_value += 1.0f;
    double_value.fetch_sub(1.0);
#endif
    integer_rmw.fetch_and(3);
    integer_rmw |= 2;
    ++integer_rmw;
    integer_rmw--;
    std::atomic_store(&value, 2u);
    if (opaque_pointer.load() != nullptr) return 5;
    opaque_pointer.store(&value);
    if (opaque_pointer.load() != &value) return 6;
    if (std::atomic_load(&value) != 2u || value.fetch_add(1u) != 2u ||
        std::atomic_fetch_add(&value, 1u) != 3u ||
        std::atomic_load(&value) != 4u)
        return 1;
    if (std::atomic_flag_test_and_set(&flag)) return 2;
    std::atomic_flag_clear(&flag);
    return 0;
}

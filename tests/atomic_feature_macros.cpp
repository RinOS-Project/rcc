#include <atomic>

template<class Left, class Right>
struct atomic_feature_is_same {
    static constexpr bool value = false;
};

template<class Type>
struct atomic_feature_is_same<Type, Type> {
    static constexpr bool value = true;
};

#if __cplusplus >= 202002L
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
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
#if __cplusplus >= 202002L
    if (flag.test() || std::atomic_flag_test(&flag)) return 3;
    if (flag.test(std::memory_order_relaxed) ||
        std::atomic_flag_test_explicit(&flag, std::memory_order_relaxed))
        return 4;
    std::atomic_signed_lock_free signed_value(1);
    std::atomic_unsigned_lock_free unsigned_value(1u);
    signed_value.fetch_add(1);
    unsigned_value.fetch_add(1u);
#endif
    std::atomic_store(&value, 2u);
    if (std::atomic_load(&value) != 2u || value.fetch_add(1u) != 2u ||
        std::atomic_fetch_add(&value, 1u) != 3u ||
        std::atomic_load(&value) != 4u)
        return 1;
    if (std::atomic_flag_test_and_set(&flag)) return 2;
    std::atomic_flag_clear(&flag);
    return 0;
}

#include <atomic>

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
#else
#if !defined(__cpp_lib_atomic_is_always_lock_free) || \
    __cpp_lib_atomic_is_always_lock_free != 201603L
#error "C++17 atomic is_always_lock_free feature macro mismatch"
#endif
#if defined(__cpp_lib_atomic_flag_test) || defined(__cpp_lib_atomic_ref) || \
    defined(__cpp_lib_atomic_value_initialization) || \
    defined(__cpp_lib_atomic_wait) || defined(__cpp_lib_atomic_float)
#error "C++20 atomic feature macro advertised in an earlier language mode"
#endif
#endif

int atomic_feature_macro_probe() {
    std::atomic<unsigned> value(1u);
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    std::atomic_store(&value, 2u);
    if (std::atomic_load(&value) != 2u || value.fetch_add(1u) != 2u ||
        std::atomic_fetch_add(&value, 1u) != 3u ||
        std::atomic_load(&value) != 4u)
        return 1;
    if (std::atomic_flag_test_and_set(&flag)) return 2;
    std::atomic_flag_clear(&flag);
    return 0;
}

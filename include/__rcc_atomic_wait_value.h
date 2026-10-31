#ifndef RCC_CXX_ATOMIC_WAIT_VALUE
#define RCC_CXX_ATOMIC_WAIT_VALUE

#include <stddef.h>

namespace std {

template<class T>
inline bool __rcc_atomic_wait_value_equal_v1(
    T observed, T expected) noexcept {
    __builtin_clear_padding(&observed);
    __builtin_clear_padding(&expected);
    const unsigned char* observed_bytes =
        reinterpret_cast<const unsigned char*>(&observed);
    const unsigned char* expected_bytes =
        reinterpret_cast<const unsigned char*>(&expected);
    for (size_t index = 0; index < sizeof(T); ++index) {
        if (observed_bytes[index] != expected_bytes[index]) return false;
    }
    return true;
}

}

#endif

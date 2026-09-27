#include <rcc/stdatomic.h>

int atomic_language_surface(void) {
    _Atomic(int) direct_counter;
    _Atomic long long direct_wide;
    _Atomic(void*) direct_pointer;

    direct_counter = 41;
    direct_wide = 0x0000000200000003LL;
    direct_pointer = &direct_counter;
    if (direct_counter != 41) return 1;
    if (direct_wide != 0x0000000200000003LL) return 2;
    if (direct_pointer != &direct_counter) return 3;
    return 0;
}

int atomic_language_deref(_Atomic(int)* value) {
    *value = 7;
    return *value;
}

int atomic_language_rmw(void) {
    _Atomic(int) value;
    value = 1;
    if (++value != 2) return 1;
    if (value++ != 2 || value != 3) return 2;
    value += 4;
    if (value != 7) return 3;
    value -= 2;
    return value == 5 ? 0 : 4;
}

int atomic_language_bitwise(void) {
    _Atomic unsigned char byte_value = 0xf3u;
    _Atomic unsigned short word_value = 0xf0f3u;
    _Atomic unsigned int dword_value = 0xf0f0f0f0u;
    _Atomic unsigned long long wide_value = 0xf0f0f0f0f0f0f0f0ULL;

    if ((byte_value &= 0x3fu) != 0x33u) return 1;
    if ((byte_value |= 0x80u) != 0xb3u) return 2;
    if ((byte_value ^= 0xffu) != 0x4cu) return 3;
    if ((word_value &= 0x0ff0u) != 0x00f0u) return 4;
    if ((word_value |= 0x8001u) != 0x80f1u) return 5;
    if ((word_value ^= 0x00ffu) != 0x800eu) return 6;
    if ((dword_value |= 0x01020304u) != 0xf1f2f3f4u) return 7;
    if ((dword_value ^= 0xffffffffu) != 0x0e0d0c0bu) return 8;
    if ((wide_value ^= 0x0f0f0f0f0f0f0f0fULL) !=
        0xffffffffffffffffULL) return 9;
    return 0;
}

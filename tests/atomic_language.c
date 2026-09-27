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
    _Atomic long long wide_values[1];
    int index = 0;
    value = 1;
    if (++value != 2) return 1;
    if (value++ != 2 || value != 3) return 2;
    value += 4;
    if (value != 7) return 3;
    value -= 2;
    if (value != 5) return 4;
    wide_values[0] = 5LL;
    if (wide_values[index++]++ != 5LL || index != 1 ||
        wide_values[0] != 6LL) return 5;
    if (--wide_values[0] != 5LL) return 6;
    if (wide_values[0]-- != 5LL || wide_values[0] != 4LL) return 7;
    return 0;
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

int atomic_language_arithmetic(void) {
    _Atomic int signed_value = 3;
    _Atomic unsigned int unsigned_value = 100u;
    _Atomic unsigned char byte_value = 0x81u;

    if ((signed_value *= 7) != 21) return 1;
    if ((signed_value /= 3) != 7) return 2;
    if ((signed_value <<= 2) != 28) return 3;
    if ((signed_value >>= 1) != 14) return 4;
    if ((unsigned_value %= 9u) != 1u) return 5;
    if ((byte_value >>= 1) != 0x40u) return 6;
    if ((byte_value <<= 1) != 0x80u) return 7;
#if defined(__x86_64__)
    {
        _Atomic unsigned long long wide_value = 144ULL;
        if ((wide_value *= 5ULL) != 720ULL) return 8;
        if ((wide_value /= 9ULL) != 80ULL) return 9;
        if ((wide_value <<= 3) != 640ULL) return 10;
        if ((wide_value >>= 4) != 40ULL) return 11;
    }
#else
    {
        _Atomic long long wide_value = 144LL;
        int evaluations = 0;
        if ((wide_value *= (evaluations++, 5LL)) != 720LL) return 8;
        if (evaluations != 1) return 9;
        if ((wide_value /= 9LL) != 80LL) return 10;
        if ((wide_value <<= 3) != 640LL) return 11;
        if ((wide_value >>= 4) != 40LL) return 12;
        wide_value = -65LL;
        if ((wide_value /= 8LL) != -8LL) return 13;
        wide_value = -33LL;
        if ((wide_value >>= 2) != -9LL) return 14;
        wide_value = 9LL;
        evaluations = 0;
        if ((wide_value += (evaluations++, 5LL)) != 14LL) return 15;
        if (evaluations != 1) return 16;
        if ((wide_value -= 6LL) != 8LL) return 17;
    }
#endif
    return 0;
}

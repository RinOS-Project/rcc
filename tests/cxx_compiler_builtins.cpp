extern "C" int cxx_builtin_expect(int value) {
    return __builtin_expect(value, 1);
}

extern "C" int cxx_builtin_expect_probability(int value) {
    return __builtin_expect_with_probability(value, 1, 0.75);
}

extern "C" int cxx_builtin_unreachable_guard(int value) {
    if (value != 0) return __builtin_expect(19, 1);
    __builtin_unreachable();
}

extern "C" int cxx_builtin_trap_guard(int value) {
    if (value != 0) return 23;
    __builtin_trap();
}

extern "C" int cxx_scalar_builtins(int *value) {
    __builtin_prefetch(value, 0, 3);
    if (__builtin_bswap32(0x12345678u) != 0x78563412u) return 41;
    if (__builtin_clz(0x00100000u) != 11) return 42;
    if (__builtin_ctz(0x00001000u) != 12) return 43;
    if (__builtin_popcount(0xF0F00F0Fu) != 16) return 44;
    if (__builtin_clzll(1ULL) != 63) return 45;
    if (__builtin_ctzll(1ULL << 40) != 40) return 46;
    if (__builtin_popcountll(0xF00000000000000FULL) != 8) return 47;
    if (__builtin_parity(0x80000003u) != 1) return 48;
    if (__builtin_parityl(0x80000003UL) != 1) return 49;
    if (__builtin_ffs(0) != 0) return 50;
    if (__builtin_ffs(0x100) != 9) return 51;
    if (__builtin_ffsl(0x100UL) != 9) return 52;
    if (__builtin_ffsll(1ULL << 40) != 41) return 53;
    if (__builtin_clrsb(0) != 31) return 54;
    if (__builtin_clrsb(1) != 30) return 55;
    if (__builtin_clrsbll(1LL) != 62) return 56;
    return *value;
}

extern "C" int cxx_builtin_object_size(void) {
    char local[10];
    const char *pointer = local;
    if (__builtin_object_size(local, 0) != 10) return 61;
    if (__builtin_object_size("RinOS", 0) != 6) return 62;
    if (__builtin_object_size(pointer, 2) != 0) return 63;
    return 0;
}

extern "C" int cxx_builtin_overflow(void) {
    int result = 0;
    if (!__builtin_add_overflow(0x7fffffff, 1, &result)) return 71;
    if (result != (-2147483647 - 1)) return 72;
    if (!__builtin_mul_overflow(0x7fffffff, 2, &result)) return 73;
    return 0;
}

extern "C" int main(void) {
    if (cxx_builtin_expect(31) != 31) return 1;
    if (cxx_builtin_expect_probability(31) != 31) return 2;
    if (cxx_builtin_unreachable_guard(1) != 19) return 3;
    if (cxx_builtin_trap_guard(1) != 23) return 4;
    int value = 37;
    if (cxx_scalar_builtins(&value) != 37) return 5;
    if (cxx_builtin_object_size() != 0) return 6;
    if (cxx_builtin_overflow() != 0) return 7;
    return 0;
}

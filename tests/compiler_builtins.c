int builtin_expect_int(int value) {
    return __builtin_expect(value, 1);
}

long long builtin_expect_wide(long long value) {
    return __builtin_expect(value, 1LL);
}

int builtin_unreachable_guard(int value) {
    if (value != 0) return __builtin_expect(17, 1);
    __builtin_unreachable();
}

int builtin_trap_guard(int value) {
    if (value != 0) return 29;
    __builtin_trap();
}

unsigned short builtin_bswap16(unsigned short value) {
    return __builtin_bswap16(value);
}

unsigned int builtin_bswap32(unsigned int value) {
    return __builtin_bswap32(value);
}

unsigned long long builtin_bswap64(unsigned long long value) {
    return __builtin_bswap64(value);
}

int builtin_bit_counts(int *value) {
    __builtin_prefetch(value, 0, 3);
    if (__builtin_clz(0x00100000u) != 11) return 31;
    if (__builtin_ctz(0x00001000u) != 12) return 32;
    if (__builtin_popcount(0xF0F00F0Fu) != 16) return 33;
    if (__builtin_clzll(1ULL) != 63) return 34;
    if (__builtin_ctzll(1ULL << 40) != 40) return 35;
    if (__builtin_popcountll(0xF00000000000000FULL) != 8) return 36;
    __builtin_prefetch(value, 1, 0);
    return *value;
}

int main(void) {
    if (builtin_expect_int(23) != 23) return 1;
    if (builtin_expect_wide(0x100000005LL) != 0x100000005LL) return 2;
    if (builtin_unreachable_guard(1) != 17) return 3;
    if (builtin_trap_guard(1) != 29) return 4;
    if (builtin_bswap16(0x1234u) != 0x3412u) return 5;
    if (builtin_bswap32(0x12345678u) != 0x78563412u) return 6;
    if (builtin_bswap64(0x0102030405060708ULL) !=
        0x0807060504030201ULL) return 7;
    {
        int value = 41;
        if (builtin_bit_counts(&value) != 41) return 8;
    }
    return 0;
}

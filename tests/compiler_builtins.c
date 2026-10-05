int builtin_expect_int(int value) {
    return __builtin_expect(value, 1);
}

int builtin_expect_probability(int value) {
    return __builtin_expect_with_probability(value, 1, 0.75);
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

int builtin_parity(unsigned int value) {
    return __builtin_parity(value);
}

int builtin_parityl(unsigned long value) {
    return __builtin_parityl(value);
}

int builtin_parityll(unsigned long long value) {
    return __builtin_parityll(value);
}

int builtin_ffs(int value) {
    return __builtin_ffs(value);
}

int builtin_ffsl(unsigned long value) {
    return __builtin_ffsl(value);
}

int builtin_ffsll(unsigned long long value) {
    return __builtin_ffsll(value);
}

int builtin_clrsb(int value) {
    return __builtin_clrsb(value);
}

int builtin_clrsbl(long value) {
    return __builtin_clrsbl(value);
}

int builtin_clrsbll(long long value) {
    return __builtin_clrsbll(value);
}

unsigned long long builtin_object_size_checks(void) {
    char local[16];
    unsigned char *pointer = (unsigned char *)local;
    if (__builtin_object_size(local, 0) != 16) return 1;
    if (__builtin_object_size(&local[4], 0) != 12) return 2;
    if (__builtin_object_size("RinOS", 0) != 6) return 3;
    if (__builtin_object_size(pointer, 0) != (unsigned long)-1) {
        return 4;
    }
    if (__builtin_object_size(pointer, 2) != 0) return 5;
    return 0;
}

int builtin_strlen_check(void) {
    return (int)__builtin_strlen("RinOS");
}

int builtin_overflow_checks(void) {
    int signed_result = 0;
    unsigned int unsigned_result = 0;
    if (__builtin_add_overflow(10, 20, &signed_result) ||
        signed_result != 30) return 1;
    if (!__builtin_add_overflow(0x7fffffff, 1, &signed_result) ||
        signed_result != (-2147483647 - 1)) return 2;
    if (!__builtin_add_overflow(0xffffffffu, 1u, &unsigned_result) ||
        unsigned_result != 0u) return 3;
    if (__builtin_sub_overflow(30, 10, &signed_result) ||
        signed_result != 20) return 4;
    if (!__builtin_sub_overflow(0u, 1u, &unsigned_result) ||
        unsigned_result != 0xffffffffu) return 5;
    if (__builtin_mul_overflow(1000, 20, &signed_result) ||
        signed_result != 20000) return 6;
    if (!__builtin_mul_overflow(0xffffffffu, 2u, &unsigned_result) ||
        unsigned_result != 0xfffffffeu) return 7;
    return 0;
}

int main(void) {
    if (builtin_expect_int(23) != 23) return 1;
    if (builtin_expect_probability(23) != 23) return 2;
    if (builtin_expect_wide(0x100000005LL) != 0x100000005LL) return 3;
    if (builtin_unreachable_guard(1) != 17) return 4;
    if (builtin_trap_guard(1) != 29) return 5;
    if (builtin_bswap16(0x1234u) != 0x3412u) return 6;
    if (builtin_bswap32(0x12345678u) != 0x78563412u) return 7;
    if (builtin_bswap64(0x0102030405060708ULL) !=
        0x0807060504030201ULL) return 8;
    {
        int value = 41;
        if (builtin_bit_counts(&value) != 41) return 9;
    }
    if (builtin_parity(0x80000003u) != 1) return 10;
    if (builtin_parityl(0x80000003UL) != 1) return 11;
    if (builtin_parityll(0x8000000000000003ULL) != 1) return 12;
    if (builtin_ffs(0) != 0) return 13;
    if (builtin_ffs(0x100) != 9) return 14;
    if (builtin_ffsl(0x100UL) != 9) return 15;
    if (builtin_ffsll(1ULL << 40) != 41) return 16;
    if (builtin_clrsb(0) != 31) return 17;
    if (builtin_clrsb(1) != 30) return 18;
    if (builtin_clrsb(-2) != 30) return 19;
    if (builtin_clrsbll(0) != 63) return 20;
    if (builtin_clrsbll(1) != 62) return 21;
    if (builtin_clrsbll(-2) != 62) return 22;
    if (builtin_clrsbl(1L) != (int)(sizeof(long) * 8 - 2)) return 23;
    if (builtin_object_size_checks() != 0) return 24;
    if (builtin_overflow_checks() != 0) return 25;
    if (builtin_strlen_check() != 5) return 26;
    return 0;
}

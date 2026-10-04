int invalid_builtin_expect(void) {
    return __builtin_expect(1, 1.0);
}

void invalid_builtin_trap(void) {
    __builtin_trap(1);
}

int invalid_builtin_bswap(void) {
    return __builtin_bswap16(1ULL);
}

int invalid_builtin_clz(void) {
    return __builtin_clz(1LL);
}

void invalid_builtin_prefetch(int *value) {
    __builtin_prefetch(value, 2, 3);
}

int invalid_builtin_parity(void) {
    return __builtin_parity(1LL);
}

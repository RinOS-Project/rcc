int invalid_builtin_expect(void) {
    return __builtin_expect(1, 1.0);
}

int invalid_builtin_expect_probability(void) {
    return __builtin_expect_with_probability(1, 1, 2.0);
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

int invalid_builtin_constant_p(void) {
    return __builtin_constant_p(1, 2);
}

int invalid_builtin_object_size_arity(void) {
    return __builtin_object_size((char *)0);
}

int invalid_builtin_object_size_mode(void) {
    return (int)__builtin_object_size((char *)0, 4);
}

int invalid_builtin_strlen_pointer(const char* value) {
    return (int)__builtin_strlen(value);
}

int invalid_builtin_add_overflow_pointer(void) {
    return __builtin_add_overflow(1, 2, 3);
}

int invalid_builtin_add_overflow_types(void) {
    int result = 0;
    return __builtin_add_overflow(1u, 2u, &result);
}

int invalid_builtin_parity(void) {
    return __builtin_parity(1LL);
}

int invalid_builtin_ffs(void) {
    return __builtin_ffs(1LL);
}

int invalid_builtin_clrsb(void) {
    return __builtin_clrsb(1LL);
}

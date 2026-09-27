int invalid_builtin_expect(void) {
    return __builtin_expect(1, 1.0);
}

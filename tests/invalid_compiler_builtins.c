int invalid_builtin_expect(void) {
    return __builtin_expect(1, 1.0);
}

void invalid_builtin_trap(void) {
    __builtin_trap(1);
}

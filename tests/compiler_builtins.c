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

int main(void) {
    if (builtin_expect_int(23) != 23) return 1;
    if (builtin_expect_wide(0x100000005LL) != 0x100000005LL) return 2;
    if (builtin_unreachable_guard(1) != 17) return 3;
    if (builtin_trap_guard(1) != 29) return 4;
    return 0;
}

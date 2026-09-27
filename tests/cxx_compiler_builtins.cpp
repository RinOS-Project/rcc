extern "C" int cxx_builtin_expect(int value) {
    return __builtin_expect(value, 1);
}

extern "C" int cxx_builtin_unreachable_guard(int value) {
    if (value != 0) return __builtin_expect(19, 1);
    __builtin_unreachable();
}

extern "C" int main(void) {
    if (cxx_builtin_expect(31) != 31) return 1;
    if (cxx_builtin_unreachable_guard(1) != 19) return 2;
    return 0;
}

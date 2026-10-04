extern "C" int verified_cxx_builtin_expect(int value)
{
    return __builtin_expect(value, 1);
}

extern "C" int verified_cxx_builtin_unreachable(int value)
{
    if (value != 0) return 19;
    __builtin_unreachable();
}

extern "C" int verified_cxx_builtin_trap(int value)
{
    if (value != 0) return 23;
    __builtin_trap();
}

extern "C" int main(void)
{
    if (verified_cxx_builtin_expect(31) != 31) return 1;
    if (verified_cxx_builtin_unreachable(1) != 19) return 2;
    if (verified_cxx_builtin_trap(1) != 23) return 3;
    return 0;
}

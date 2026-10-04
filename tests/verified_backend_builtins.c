int verified_builtin_expect(int value)
{
    return __builtin_expect(value, 1);
}

int verified_builtin_unreachable(int value)
{
    if (value != 0) return 17;
    __builtin_unreachable();
}

int verified_builtin_trap(int value)
{
    if (value != 0) return 29;
    __builtin_trap();
}

int main(void)
{
    if (verified_builtin_expect(23) != 23) return 1;
    if (verified_builtin_unreachable(1) != 17) return 2;
    if (verified_builtin_trap(1) != 29) return 3;
    return 0;
}

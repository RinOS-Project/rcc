void invalid_constraint(int value)
{
    int result;
    __asm__ __volatile__("nop" : "=k"(result) : "a"(value));
}

void invalid_output(int value)
{
    __asm__ __volatile__("nop" : "=a"(value + 1));
}

void invalid_clobber(void)
{
    __asm__ __volatile__("nop" : : : "not_a_register");
}

void invalid_placeholder(int value)
{
    __asm__ __volatile__("nop %0" : : "a"(value));
}

void invalid_type(double value)
{
    __asm__ __volatile__("nop" : : "a"(value));
}

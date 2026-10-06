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
    __asm__ __volatile__("nop %x0" : : "a"(value));
}

void invalid_placeholder_index(int value)
{
    __asm__ __volatile__("nop %9" : : "a"(value));
}

void invalid_type(double value)
{
    __asm__ __volatile__("nop" : : "a"(value));
}

void invalid_immediate(int value)
{
    __asm__ __volatile__("int %0" : : "i"(value));
}

void invalid_duplicate_output(int left, int right)
{
    int first;
    int second;
    __asm__ __volatile__("nop" : "=a"(first), "=a"(second));
}

void invalid_duplicate_input(int left, int right)
{
    __asm__ __volatile__("nop" : : "a"(left), "a"(right));
}

void invalid_duplicate_generic_input(int left, int right)
{
    __asm__ __volatile__("nop" : : "r"(left), "r"(right));
}

void invalid_clobber_conflict(int value)
{
    __asm__ __volatile__("nop" : : "a"(value) : "eax");
}

void invalid_duplicate_clobber(void)
{
    __asm__ __volatile__("nop" : : : "eax", "eax");
}

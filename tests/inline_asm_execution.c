/* Fixed-register inline asm must survive later operand evaluation. */

int asm_fixed_register_roundtrip(int value, int distraction)
{
    int result;
    __asm__ __volatile__("nop"
                         : "=a"(result)
                         : "a"(value), "D"(distraction));
    return result;
}

int asm_read_write_accumulator(int value)
{
    __asm__ __volatile__("nop" : "+a"(value));
    return value;
}

int asm_placeholder_move(int value)
{
    int result;
    __asm__ __volatile__("mov %1, %0" : "=a"(result) : "b"(value));
    return result;
}

int asm_generic_placeholder_move(int value)
{
    int result;
    __asm__ __volatile__("mov %1, %0" : "=a"(result) : "r"(value));
    return result;
}

int asm_generic_output_move(int value)
{
    int result;
    __asm__ __volatile__("mov %1, %0" : "=r"(result) : "a"(value));
    return result;
}

int asm_generic_read_write(int value)
{
    __asm__ __volatile__("mov %0, %0" : "+r"(value));
    return value;
}

int asm_general_input_move(int value)
{
    int result;
    __asm__ __volatile__("mov %1, %0" : "=a"(result) : "g"(value));
    return result;
}

int asm_general_output_move(int value)
{
    int result;
    __asm__ __volatile__("mov %1, %0" : "=g"(result) : "a"(value));
    return result;
}

int asm_general_read_write(int value)
{
    __asm__ __volatile__("mov %0, %0" : "+g"(value));
    return value;
}

void asm_immediate_interrupt(void)
{
    __asm__ __volatile__("int %0" : : "i"(0x40 + 0x40));
}

void asm_c_modifier_interrupt(void)
{
    __asm__ __volatile__("int $%c0" : : "i"(0x80));
}

void asm_n_modifier_interrupt(void)
{
    __asm__ __volatile__("int $%n0" : : "i"(5));
}

int asm_callee_saved_clobber(int value)
{
#if defined(__x86_64__)
    __asm__ __volatile__("nop" : : "a"(value) : "rbx");
#else
    __asm__ __volatile__("nop" : : "a"(value) : "ebx", "esi", "edi");
#endif
    return value;
}

long asm_syscall3(long number, long first, long second, long third)
{
    long result;
#if defined(__x86_64__)
    __asm__ __volatile__("syscall"
                         : "=a"(result)
                         : "a"(number), "D"(first), "S"(second), "d"(third)
                         : "rcx", "r11", "memory");
#else
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(number), "b"(first), "c"(second), "d"(third)
                         : "memory");
#endif
    return result;
}

#if defined(__x86_64__)
long long asm_x64_q_modifier(long long value)
{
    long long result;
    __asm__ __volatile__("mov %q1, %q0" : "=a"(result) : "b"(value));
    return result;
}
#endif

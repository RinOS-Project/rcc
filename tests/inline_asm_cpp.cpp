#if defined(__i386__)
extern "C" int asm_cpp_memory_input_move(int value)
{
    int result;
    asm volatile("movl %1, %0" : "=a"(result) : "m"(value));
    return result;
}
#endif

extern "C" int asm_cpp_basic_percent_register(int value)
{
    asm volatile("movl %esp, %esp");
    return value;
}

extern "C" int asm_cpp_placeholder_move(int value)
{
    int result;
    asm volatile("mov %1, %0" : "=a"(result) : "b"(value));
    return result;
}

extern "C" int asm_cpp_generic_output_move(int value)
{
    int result;
    asm volatile("mov %1, %0" : "=r"(result) : "a"(value));
    return result;
}

extern "C" int asm_cpp_earlyclobber_generic(int first, int later)
{
    int result;
    asm volatile("mov %1, %0\n\tmov %2, %0"
                 : "=&r"(result) : "a"(first), "r"(later));
    return result;
}

extern "C" int asm_cpp_earlyclobber_generic_read_write(int first, int later)
{
    asm volatile("mov $0, %0\n\tmov %1, %0"
                 : "+&r"(first) : "r"(later));
    return first;
}

extern "C" unsigned int asm_cpp_mov_imm8(int unused)
{
    unsigned char result;
    (void)unused;
    asm volatile("mov $0x7f, %b0" : "=a"(result));
    return result;
}

extern "C" unsigned int asm_cpp_mov_imm16(int unused)
{
    unsigned short result;
    (void)unused;
    asm volatile("mov $0x1234, %w0" : "=a"(result));
    return result;
}

extern "C" unsigned int asm_cpp_mov_imm32(int unused)
{
    unsigned int result;
    (void)unused;
    asm volatile("mov $0x12345678, %k0" : "=a"(result));
    return result;
}

#if defined(__x86_64__)
extern "C" unsigned long long asm_cpp_mov_imm64(long long unused)
{
    unsigned long long result;
    (void)unused;
    asm volatile("mov $0x1122334455667788, %0" : "=a"(result));
    return result;
}
#endif

extern "C" int asm_cpp_generic_read_write(int value)
{
    asm volatile("mov %0, %0" : "+r"(value));
    return value;
}

extern "C" int asm_cpp_general_input_move(int value)
{
    int result;
    asm volatile("mov %1, %0" : "=a"(result) : "g"(value));
    return result;
}

extern "C" int asm_cpp_general_output_move(int value)
{
    int result;
    asm volatile("mov %1, %0" : "=g"(result) : "a"(value));
    return result;
}

extern "C" int asm_cpp_general_read_write(int value)
{
    asm volatile("mov %0, %0" : "+g"(value));
    return value;
}

extern "C" void asm_cpp_immediate_interrupt()
{
    asm volatile("int %0" : : "n"(0x80));
}

extern "C" void asm_cpp_c_modifier_interrupt()
{
    asm volatile("int $%c0" : : "i"(0x80));
}

extern "C" void asm_cpp_n_modifier_interrupt()
{
    asm volatile("int $%n0" : : "i"(5));
}

#if defined(__x86_64__)
extern "C" long long asm_cpp_x64_q_modifier(long long value)
{
    long long result;
    asm volatile("mov %q1, %q0" : "=a"(result) : "b"(value));
    return result;
}
#endif

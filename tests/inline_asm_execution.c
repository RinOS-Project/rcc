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

int asm_gnu_register_variable_move(int value)
{
    int result;
#if defined(__x86_64__)
    register int fixed __asm__("r8") = value;
#else
    register int fixed __asm__("ebx") = value;
#endif
    __asm__ __volatile__("mov %1, %0" : "=a"(result) : "r"(fixed));
    return result;
}

#if defined(__x86_64__)
long long asm_gnu_register_variable_r9(long long value)
{
    long long result;
    register long long fixed __asm__("r9") = value;
    __asm__ __volatile__("mov %1, %0" : "=a"(result) : "r"(fixed));
    return result;
}

long long asm_gnu_register_variable_r10(long long value)
{
    long long result;
    register long long fixed __asm__("r10") = value;
    __asm__ __volatile__("mov %1, %0" : "=a"(result) : "r"(fixed));
    return result;
}
#endif

int asm_generic_output_move(int value)
{
    int result;
    __asm__ __volatile__("mov %1, %0" : "=r"(result) : "a"(value));
    return result;
}

int asm_earlyclobber_generic(int first, int later)
{
    int result;
    __asm__ __volatile__("mov %1, %0\n\tmov %2, %0"
                         : "=&r"(result) : "a"(first), "r"(later));
    return result;
}

int asm_earlyclobber_generic_avoids_fixed(int first, int later)
{
    int result;
#if defined(__i386__)
    __asm__ __volatile__("mov %1, %0\n\tmov %2, %0"
                         : "=&r"(result) : "d"(first), "r"(later));
#else
    __asm__ __volatile__("mov %1, %0\n\tmov %2, %0"
                         : "=&r"(result) : "c"(first), "r"(later));
#endif
    return result;
}

int asm_earlyclobber_generic_read_write(int first, int later)
{
    __asm__ __volatile__("mov $0, %0\n\tmov %1, %0"
                         : "+&r"(first) : "r"(later));
    return first;
}

unsigned int asm_mov_imm8(int unused)
{
    unsigned char result;
    (void)unused;
    __asm__ __volatile__("mov $0x7f, %b0" : "=a"(result));
    return result;
}

unsigned int asm_mov_imm16(int unused)
{
    unsigned short result;
    (void)unused;
    __asm__ __volatile__("mov $0x1234, %w0" : "=a"(result));
    return result;
}

unsigned int asm_mov_imm32(int unused)
{
    unsigned int result;
    (void)unused;
    __asm__ __volatile__("mov $0x12345678, %k0" : "=a"(result));
    return result;
}

#if !defined(__i386__)
unsigned long long asm_mov_imm64(long long unused)
{
    unsigned long long result;
    (void)unused;
    __asm__ __volatile__("mov $0x1122334455667788, %0" : "=a"(result));
    return result;
}
#endif

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

#if defined(__i386__)
int asm_memory_input_move(int value)
{
    int result;
    __asm__ __volatile__("movl %1, %0" : "=a"(result) : "m"(value));
    return result;
}

int asm_base_memory_move(int* source)
{
    int result;
    __asm__ __volatile__("mov (%1), %0" : "=c"(result) : "a"(source));
    return result;
}

void asm_base_memory_store(int* destination, int value)
{
    __asm__ __volatile__("mov %1, (%0)" : : "b"(destination), "a"(value));
}

int* asm_base_memory_lea(int* base)
{
    int* result;
    __asm__ __volatile__("lea 4(%1), %0" : "=a"(result) : "c"(base));
    return result;
}

void asm_esp_memory_move(void)
{
    __asm__ __volatile__("mov (%%esp), %%eax");
}

void asm_ebp_memory_move(void)
{
    __asm__ __volatile__("mov (%%ebp), %%eax");
}

int asm_x86_xor_test(int value, int other)
{
    __asm__ __volatile__("xor %1, %0\n\ttest %0, %0"
                         : "+a"(value) : "c"(other));
    return value;
}

void asm_x86_indirect_jump(void (*target)(void))
{
    __asm__ __volatile__("jmp *%0" : : "a"(target));
}

void asm_x86_push_pop(void)
{
    __asm__ __volatile__("pushl %%eax\n\tpopl %%eax");
}

int asm_i686_memory_branch_sequence(int* slot, int value)
{
    int result;
    __asm__ __volatile__(
        "xor %%eax, %%eax\n\t"
        "mov %1, %%eax\n\t"
        "mov %%eax, 0(%2)\n\t"
        "lea 0(%2), %%edx\n\t"
        "mov 0(%%edx), %%eax\n\t"
        "test %%eax, %%eax\n\t"
        "jnz 1f\n\t"
        "mov $7, %%eax\n\t"
        "jmp 2f\n\t"
        "1:\n\t"
        "mov $9, %%eax\n\t"
        "2:\n\t"
        "xor %%ecx, %%ecx\n\t"
        "test %%ecx, %%ecx\n\t"
        "jnz 3f\n\t"
        "jmp 4f\n\t"
        "3:\n\t"
        "jmp 3b\n\t"
        "4:\n\t"
        "pushl %%eax\n\t"
        "popl %%eax\n\t"
        "mov %%eax, %0"
        : "=D"(result)
        : "b"(value), "S"(slot)
        : "eax", "ecx", "edx", "cc", "memory");
    return result;
}

__attribute__((naked))
void asm_i686_indirect_tail(void (*target)(void) __attribute__((unused)))
{
    __asm__ __volatile__(
        "mov 4(%%esp), %%ecx\n\t"
        "jmp *%%ecx" : : : "ecx");
}
#endif

#if defined(__x86_64__)
long long asm_x64_q_modifier(long long value)
{
    long long result;
    __asm__ __volatile__("mov %q1, %q0" : "=a"(result) : "b"(value));
    return result;
}
#endif

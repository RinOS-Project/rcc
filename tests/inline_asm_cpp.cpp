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

extern "C" int asm_cpp_placeholder_move(int value)
{
    int result;
    asm volatile("mov %1, %0" : "=a"(result) : "b"(value));
    return result;
}

extern "C" void asm_cpp_immediate_interrupt()
{
    asm volatile("int %0" : : "n"(0x80));
}

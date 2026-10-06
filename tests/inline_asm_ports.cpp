extern "C" unsigned char asm_cpp_port_inb_const()
{
    unsigned char value;
    asm volatile("inb %w1, %b0" : "=a"(value) : "Nd"(0x66));
    return value;
}

extern "C" void asm_cpp_port_outb_register(unsigned char value,
                                             unsigned short port)
{
    asm volatile("outb %b0, %w1" :: "a"(value), "Nd"(port));
}

extern "C" unsigned short asm_cpp_modifier_word_move(unsigned short value)
{
    unsigned short result;
    asm volatile("mov %w1, %w0" : "=a"(result) : "b"(value));
    return result;
}

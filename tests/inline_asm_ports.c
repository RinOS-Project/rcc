unsigned char asm_port_inb_const(void)
{
    unsigned char value;
    __asm__ __volatile__("inb %w1, %b0" : "=a"(value) : "Nd"(0x60));
    return value;
}

unsigned short asm_port_inw_const(void)
{
    unsigned short value;
    __asm__ __volatile__("inw %w1, %w0" : "=a"(value) : "Nd"(0x61));
    return value;
}

unsigned int asm_port_inl_const(void)
{
    unsigned int value;
    __asm__ __volatile__("inl %w1, %k0" : "=a"(value) : "Nd"(0x62));
    return value;
}

void asm_port_outb_const(unsigned char value)
{
    __asm__ __volatile__("outb %b0, %w1" :: "a"(value), "Nd"(0x63));
}

void asm_port_outw_const(unsigned short value)
{
    __asm__ __volatile__("outw %w0, %w1" :: "a"(value), "Nd"(0x64));
}

void asm_port_outl_const(unsigned int value)
{
    __asm__ __volatile__("outl %k0, %w1" :: "a"(value), "Nd"(0x65));
}

unsigned char asm_port_inb_register(unsigned short port)
{
    unsigned char value;
    __asm__ __volatile__("inb %w1, %b0" : "=a"(value) : "Nd"(port));
    return value;
}

void asm_port_outb_register(unsigned char value, unsigned short port)
{
    __asm__ __volatile__("outb %b0, %w1" :: "a"(value), "Nd"(port));
}

unsigned char asm_modifier_byte_move(unsigned char value)
{
    unsigned char result;
    __asm__ __volatile__("mov %b1, %b0" : "=a"(result) : "b"(value));
    return result;
}

unsigned short asm_modifier_word_move(unsigned short value)
{
    unsigned short result;
    __asm__ __volatile__("mov %w1, %w0" : "=a"(result) : "b"(value));
    return result;
}

unsigned int asm_modifier_dword_move(unsigned int value)
{
    unsigned int result;
    __asm__ __volatile__("mov %k1, %k0" : "=a"(result) : "b"(value));
    return result;
}

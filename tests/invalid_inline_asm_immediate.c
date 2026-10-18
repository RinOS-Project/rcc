void invalid_inline_asm_mov_immediate_width(void)
{
    unsigned char result;
    __asm__ __volatile__("mov $0x100, %b0" : "=a"(result));
}

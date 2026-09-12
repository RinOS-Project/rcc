void invalid_inline_asm_instruction(void)
{
    __asm__ __volatile__("not_a_real_instruction");
}

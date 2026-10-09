void invalid_c_register_modifier(int value)
{
    __asm__ __volatile__("mov %c0, %%eax" : : "r"(value));
}

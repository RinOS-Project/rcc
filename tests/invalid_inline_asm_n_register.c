void invalid_n_register_modifier(int value)
{
    __asm__ __volatile__("mov %n0, %%eax" : : "r"(value));
}

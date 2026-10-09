void invalid_q_immediate_modifier(void)
{
    __asm__ __volatile__("mov %q0, %%rax" : : "i"(17));
}

void invalid_b_immediate_modifier(void)
{
    __asm__ __volatile__("mov %b0, %%rax" : : "i"(17));
}

void invalid_w_immediate_modifier(void)
{
    __asm__ __volatile__("mov %w0, %%rax" : : "i"(17));
}

void invalid_k_immediate_modifier(void)
{
    __asm__ __volatile__("mov %k0, %%rax" : : "i"(17));
}

void invalid_q_immediate_modifier(void)
{
    __asm__ __volatile__("mov %q0, %%rax" : : "i"(17));
}

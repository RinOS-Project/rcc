void invalid_amd64_memory_rvalue(void)
{
    __asm__ __volatile__("movl %0, %%eax" : : "m"(17));
}

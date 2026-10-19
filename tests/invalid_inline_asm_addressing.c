void invalid_amd64_i686_address_register(void)
{
    __asm__ __volatile__("mov (%%eax), %%ecx");
}

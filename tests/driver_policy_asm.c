int main(void)
{
    __asm__ __volatile__("movaps %xmm0, %xmm1");
    return 0;
}

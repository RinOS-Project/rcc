int main(void)
{
    __asm__ __volatile__("" : : : "xmm0");
    return 0;
}

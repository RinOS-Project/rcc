int main(void)
{
    int value;
    __asm__ __volatile__("" : "=x"(value));
    return value;
}

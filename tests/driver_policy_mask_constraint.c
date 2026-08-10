int main(void)
{
    int value;
    __asm__ __volatile__("" : "=k"(value));
    return value;
}

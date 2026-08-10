int main(void)
{
    int value = 1;
    __asm__ __volatile__("pause" : "+a"(value));
    __asm__ __volatile__("" : : "{eax}"(value));
    __asm__ __volatile__("" : : "X"(value));
    return value;
}

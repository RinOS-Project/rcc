#include <rcc/intrin.h>

unsigned long long intrin_header_cpp(unsigned short port,
                                     unsigned char value,
                                     unsigned long reg)
{
    int info[4];
    unsigned int aux = 0;
    __cpuidex(info, 0, 0);
    unsigned long long ticks = __rdtsc() + __rdtscp(&aux);
    __faststorefence();
    __outbyte(port, value);
    return ticks + (unsigned long long)info[0] + aux + reg;
}

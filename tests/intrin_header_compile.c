#include <rcc/intrin.h>

unsigned long long intrin_header_all(unsigned short port,
                                     unsigned char value,
                                     unsigned long reg)
{
    int info[4];
    unsigned int aux = 0;
    unsigned long long ticks;
    unsigned long long control;
    unsigned long long msr;

    __cpuidex(info, 0, 0);
    ticks = __rdtsc() + __rdtscp(&aux);
    __faststorefence();
    msr = __readmsr(reg);
    __writemsr(reg, ticks);
    control = __readcr0() + __readcr2() + __readcr3() + __readcr4();
    __writecr0(control);
    __writecr3(control);
    __writecr4(control);
    __outbyte(port, value);
    return ticks + msr + control + (unsigned long long)info[0] + aux;
}

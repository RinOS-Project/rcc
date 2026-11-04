#include <stdint.h>

#if defined(RCC_EXPECT_SSE2) && !defined(__SSE2__)
#error "-msse2 must define __SSE2__"
#endif

typedef double test_v2df __attribute__((__vector_size__(16)));
typedef long long test_v2di __attribute__((__vector_size__(16)));

_Alignas(16) static double pd_destination[2];
_Alignas(16) static test_v2di dq_destination;

void store_ntpd(void) {
    test_v2df pd_source = {7.25, -8.5};
    __builtin_ia32_movntpd(&pd_destination[0], pd_source);
}

void store_ntdq(void) {
    test_v2di dq_source = {0x123456789LL, -0x12345LL};
    __builtin_ia32_movntdq(&dq_destination, dq_source);
}

int main(void) {
    if (((uintptr_t)pd_destination & 15u) != 0u ||
        ((uintptr_t)&dq_destination & 15u) != 0u) {
        return 1;
    }
    store_ntpd();
    store_ntdq();
    __builtin_ia32_sfence();
    if (pd_destination[0] != 7.25 || pd_destination[1] != -8.5 ||
        dq_destination[0] != 0x123456789LL ||
        dq_destination[1] != -0x12345LL) {
        return 2;
    }
    return 0;
}

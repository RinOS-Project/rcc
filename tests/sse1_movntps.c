#include <stdint.h>

#if defined(RCC_EXPECT_SSE1) && !defined(__SSE__)
#error "-msse must define __SSE__"
#endif
#if defined(RCC_EXPECT_SSE1) && defined(__SSE2__)
#error "this fixture must compile without SSE2"
#endif

typedef float test_v4sf __attribute__((__vector_size__(16)));

_Alignas(16) static float destination[4];

void store_ntps(void) {
    test_v4sf source = {1.25f, -2.5f, 3.75f, -5.0f};
    __builtin_ia32_movntps(&destination[0], source);
}

int main(void) {
    if (((uintptr_t)destination & 15u) != 0u) return 1;
    store_ntps();
    __builtin_ia32_sfence();
    if (destination[0] != 1.25f || destination[1] != -2.5f ||
        destination[2] != 3.75f || destination[3] != -5.0f) {
        return 2;
    }
    return 0;
}

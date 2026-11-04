#include <emmintrin.h>

#if !defined(__x86_64__) || !defined(__SSE2__)
#error "_mm_stream_si64 requires an x86_64 target with SSE2 enabled"
#endif

int main(void) {
    _Alignas(8) long long destination = 0;
    _mm_stream_si64(&destination, 0x123456789abcdef0LL);
    _mm_sfence();
    return destination == 0x123456789abcdef0LL ? 0 : 1;
}

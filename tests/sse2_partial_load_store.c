#include <emmintrin.h>

_Static_assert(_Alignof(__m128i_u) == 1,
               "__m128i_u must preserve unaligned pointer access");

#if !defined(__SSE2__)
#error "-msse2 must define __SSE2__"
#endif

void rcc_loadu_si16(const void* source, void* result) {
    _mm_storeu_si128((__m128i*)result, _mm_loadu_si16(source));
}

void rcc_loadu_si32(const void* source, void* result) {
    _mm_storeu_si128((__m128i*)result, _mm_loadu_si32(source));
}

void rcc_loadu_si64(const void* source, void* result) {
    _mm_storeu_si128((__m128i*)result, _mm_loadu_si64(source));
}

void rcc_loadl_epi64(const void* source, void* result) {
    _mm_storeu_si128((__m128i*)result,
                     _mm_loadl_epi64((const __m128i_u*)source));
}

void rcc_storeu_si16(void* destination, const void* source) {
    _mm_storeu_si16(destination,
                    _mm_loadu_si128((const __m128i*)source));
}

void rcc_storeu_si32(void* destination, const void* source) {
    _mm_storeu_si32(destination,
                    _mm_loadu_si128((const __m128i*)source));
}

void rcc_storeu_si64(void* destination, const void* source) {
    _mm_storeu_si64(destination,
                    _mm_loadu_si128((const __m128i*)source));
}

void rcc_storel_epi64(void* destination, const void* source) {
    _mm_storel_epi64((__m128i_u*)destination,
                     _mm_loadu_si128((const __m128i*)source));
}

#ifndef RCC_PARTIAL_LOAD_STORE_NO_MAIN
static int load_matches(const unsigned char* result,
                        const unsigned char* expected, int width) {
    for (int index = 0; index < 16; ++index) {
        unsigned char value = index < width ? expected[index] : 0;
        if (result[index] != value) return 0;
    }
    return 1;
}

static int store_matches(const unsigned char* result,
                         const unsigned char* source, int offset, int width) {
    for (int index = 0; index < 32; ++index) {
        unsigned char value = 0xA5;
        if (index >= offset && index < offset + width) {
            value = source[1 + index - offset];
        }
        if (result[index] != value) return 0;
    }
    return 1;
}

static void fill_bytes(unsigned char* destination, unsigned char value,
                       int length) {
    for (int index = 0; index < length; ++index) {
        destination[index] = value;
    }
}

int main(void) {
    unsigned char input[32];
    unsigned char result[16];
    unsigned char vector_source[32];
    unsigned char output[32];

    for (int index = 0; index < 32; ++index) {
        input[index] = (unsigned char)(index * 7 + 3);
        vector_source[index] = (unsigned char)(index * 11 + 5);
    }

    fill_bytes(result, 0xCC, (int)sizeof(result));
    rcc_loadu_si16(input + 1, result);
    if (!load_matches(result, input + 1, 2)) return 1;
    fill_bytes(result, 0xCC, (int)sizeof(result));
    rcc_loadu_si32(input + 3, result);
    if (!load_matches(result, input + 3, 4)) return 2;
    fill_bytes(result, 0xCC, (int)sizeof(result));
    rcc_loadu_si64(input + 5, result);
    if (!load_matches(result, input + 5, 8)) return 3;
    fill_bytes(result, 0xCC, (int)sizeof(result));
    rcc_loadl_epi64(input + 7, result);
    if (!load_matches(result, input + 7, 8)) return 4;

    for (int index = 0; index < 32; ++index) output[index] = 0xA5;
    rcc_storeu_si16(output + 1, vector_source + 1);
    if (!store_matches(output, vector_source, 1, 2)) return 5;
    for (int index = 0; index < 32; ++index) output[index] = 0xA5;
    rcc_storeu_si32(output + 3, vector_source + 1);
    if (!store_matches(output, vector_source, 3, 4)) return 6;
    for (int index = 0; index < 32; ++index) output[index] = 0xA5;
    rcc_storeu_si64(output + 5, vector_source + 1);
    if (!store_matches(output, vector_source, 5, 8)) return 7;
    for (int index = 0; index < 32; ++index) output[index] = 0xA5;
    rcc_storel_epi64(output + 7, vector_source + 1);
    if (!store_matches(output, vector_source, 7, 8)) return 8;
    return 0;
}
#endif

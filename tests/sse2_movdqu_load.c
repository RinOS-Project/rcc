#if defined(RCC_EXPECT_SSE2) && !defined(__SSE2__)
#error "-msse2 must define __SSE2__"
#endif

void movdqu_copy(const char* source, char* destination) {
    __builtin_ia32_storedqu(destination, __builtin_ia32_loaddqu(source));
}

#ifndef RCC_MOVDQU_NO_MAIN
int main(void) {
    _Alignas(16) char source[64];
    char destination[16];

    for (int i = 0; i < 64; ++i) source[i] = (char)(i * 7 + 3);
    for (int offset = 0; offset <= 16; ++offset) {
        movdqu_copy(source + offset, destination);
        for (int i = 0; i < 16; ++i) {
            if (destination[i] != source[offset + i]) return 1;
        }
    }
    return 0;
}
#endif

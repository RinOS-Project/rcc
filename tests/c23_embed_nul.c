static const unsigned char embedded_nul[] = {
#embed "../build/tests/c23-embed/nul.bin"
};

_Static_assert(sizeof(embedded_nul) / sizeof(embedded_nul[0]) == 3,
               "embedded NUL bytes must not truncate the resource");

int main(void) {
    return embedded_nul[0] != 0 || embedded_nul[1] != 65 ||
           embedded_nul[2] != 255;
}

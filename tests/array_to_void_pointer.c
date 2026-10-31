typedef unsigned long size_t;

void* rcc_test_memset(void*, int, size_t);
void* rcc_test_memcpy(void*, const void*, size_t);

void rcc_test_array_to_void_pointer(void) {
    unsigned char destination[3][7];
    const unsigned char source[3][7] = {{0}};
    rcc_test_memset(destination, 0, sizeof(destination));
    rcc_test_memcpy(destination, source, sizeof(destination));
}

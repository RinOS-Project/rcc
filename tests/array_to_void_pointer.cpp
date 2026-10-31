using size_t = unsigned long;

void* rcc_test_memset(void*, int, size_t);
void* rcc_test_memcpy(void*, const void*, size_t);

void rcc_test_array_to_void_pointer() {
    unsigned char destination[3][7];
    const unsigned char source[3][7] = {{0}};
    rcc_test_memset(destination, 0, sizeof(destination));
    rcc_test_memcpy(destination, source, sizeof(destination));
}

struct rcc_test_lconv;
extern "C" struct rcc_test_lconv* rin_localeconv(void);

#if defined(_WIN32) && defined(__x86_64__)
#define RINOS_TEST_ABI __attribute__((sysv_abi))
#else
#define RINOS_TEST_ABI
#endif

extern int rcc_test_main(void) RINOS_TEST_ABI;

int main(void) {
    return rcc_test_main();
}

/* Native-host adapter for C++ fixtures whose generated main has no CRT ABI. */
extern int rcc_test_main(void);

int main(void) {
    return rcc_test_main();
}

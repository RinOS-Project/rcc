/* Host harness for the C++ comma-declarator regression. */

extern void __rcc_global_init(void);
extern int cxx_multi_declarator_main(void);

int main(void) {
    __rcc_global_init();
    return cxx_multi_declarator_main();
}

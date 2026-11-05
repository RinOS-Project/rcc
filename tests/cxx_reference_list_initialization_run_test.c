int rcc_generated_main(void);
void __rcc_global_init(void);
extern int global_reference_initializer_calls;

int main(void) {
    /* ELF CRTs may dispatch .init_array, while MinGW does not. */
    if (global_reference_initializer_calls == 0) __rcc_global_init();
    return rcc_generated_main();
}

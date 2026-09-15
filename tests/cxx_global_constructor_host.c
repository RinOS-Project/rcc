/* Host harness for the generated C++ global-constructor callback. */

extern void __rcc_global_init(void);
extern int rcc_cxx_global_constructor_main(void);
extern int cxx_global_constructor_calls;

int main(void) {
    /* ELF CRTs may dispatch .init_array, while the RinOS loader calls the
     * same callback explicitly.  Keep the host harness single-shot in both
     * environments. */
    if (cxx_global_constructor_calls == 0) __rcc_global_init();
    return rcc_cxx_global_constructor_main();
}

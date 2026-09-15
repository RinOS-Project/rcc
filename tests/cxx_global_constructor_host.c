/* Host harness for the generated C++ global-constructor callback. */

extern void __rcc_global_init(void);
extern void __rcc_global_fini(void);
extern int rcc_cxx_global_constructor_main(void);
extern int cxx_global_constructor_calls;
extern int cxx_global_destructor_calls;

int main(void) {
    int result;
    /* ELF CRTs may dispatch .init_array, while the RinOS loader calls the
     * same callback explicitly.  Keep the host harness single-shot in both
     * environments. */
    if (cxx_global_constructor_calls == 0) __rcc_global_init();
    result = rcc_cxx_global_constructor_main();
    if (result != 0) return result;
    __rcc_global_fini();
    return cxx_global_destructor_calls == 1 ? 0 : 1;
}

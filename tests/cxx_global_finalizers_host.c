/* Host harness for the generated static-storage cleanup callback. */

extern void __rcc_global_fini(void);
extern int rcc_global_finalizers_main(void);
extern int global_cleanup_count;
extern int global_cleanup_sequence;
extern int global_storage;
extern int second_storage;

int main(void) {
    int result;
    result = rcc_global_finalizers_main();
    if (result != 0) return result;
    __rcc_global_fini();
    return global_cleanup_count == 2 && global_cleanup_sequence == 79 &&
                   global_storage == 0 && second_storage == 0 ? 0 : 1;
}

int rcc_generated_main(void);
int __rcc_global_init(void);

int main(void) {
    int result = __rcc_global_init();
    if (result != 0) return result;
    return rcc_generated_main();
}

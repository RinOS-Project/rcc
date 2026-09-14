/* Host harness for the generated global-initializer callback. */

extern void __rcc_global_init(void);
extern int rcc_global_initializers_main(void);

int main(void) {
    __rcc_global_init();
    return rcc_global_initializers_main();
}

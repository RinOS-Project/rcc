#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

extern int RIN_SYSV decltype_auto_local_probe(void);
extern void RIN_SYSV __rcc_global_init(void) __attribute__((weak));

int main(void) {
    if (__rcc_global_init) __rcc_global_init();
    return decltype_auto_local_probe() == 123 ? 0 : 1;
}

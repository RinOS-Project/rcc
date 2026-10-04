#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

extern int RIN_SYSV auto_local_reference_probe(void);
extern int RIN_SYSV auto_local_const_probe(void);
extern int RIN_SYSV auto_local_pointer_probe(void);
extern int RIN_SYSV auto_local_direct_list_probe(void);

int main(void) {
    if (auto_local_reference_probe() != 16) return 1;
    if (auto_local_const_probe() != 42) return 2;
    if (auto_local_pointer_probe() != 8) return 3;
    return auto_local_direct_list_probe() == 13 ? 0 : 4;
}

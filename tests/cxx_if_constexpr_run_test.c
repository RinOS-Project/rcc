#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

extern int RIN_SYSV probe_if_constexpr(void);

int main(void) {
    return probe_if_constexpr() == 42 ? 0 : 1;
}

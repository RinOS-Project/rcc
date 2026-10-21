struct VerifiedArrowTarget {
    int value;
};

struct VerifiedArrowRoot {
    struct VerifiedArrowTarget* target;
};

#if defined(_WIN32) && defined(__x86_64__) && \
    !defined(TEST_GCC_NATIVE_ABI)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

#ifdef __cplusplus
extern "C" {
#endif
extern int verified_arrow_cleanup_count;
extern int RIN_SYSV verified_arrow_read(struct VerifiedArrowRoot* root);
#ifdef __cplusplus
}
#endif

int main(void)
{
    struct VerifiedArrowTarget target = {41};
    struct VerifiedArrowRoot root = {&target};
    if (verified_arrow_read(&root) != 0) return 1;
    if (verified_arrow_cleanup_count != 1) return 2;
    return 0;
}

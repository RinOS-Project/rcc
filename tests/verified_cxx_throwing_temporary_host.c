#include <stdio.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RINOS_TEST_ABI __attribute__((sysv_abi))
#else
#define RINOS_TEST_ABI
#endif

extern int verified_cxx_throwing_temporary_cleanup(int should_throw)
    RINOS_TEST_ABI;
extern int verified_cxx_throwing_argument_cleanup(void) RINOS_TEST_ABI;
extern int verified_cxx_throwing_conditional_cleanup(
    int select_outer, int select_inner)
    RINOS_TEST_ABI;
extern int verified_cxx_local_reference_normal_cleanup(void) RINOS_TEST_ABI;
extern int rcc_test_main(void) RINOS_TEST_ABI;

int main(void) {
    int result = verified_cxx_throwing_temporary_cleanup(1);
    if (result != 472) {
        fprintf(stderr, "throwing temporary cleanup returned %d, expected 472\n",
                result);
        return 1;
    }
    result = verified_cxx_throwing_temporary_cleanup(0);
    if (result != 22) {
        fprintf(stderr, "normal temporary cleanup returned %d, expected 22\n",
                result);
        return 2;
    }
    result = verified_cxx_throwing_argument_cleanup();
    if (result != 533) {
        fprintf(stderr,
                "throwing argument cleanup returned %d, expected 533\n",
                result);
        return 3;
    }
    result = verified_cxx_throwing_conditional_cleanup(0, 1);
    if (result != 533) {
        fprintf(stderr,
                "throwing conditional cleanup returned %d, expected 533\n",
                result);
        return 4;
    }
    result = verified_cxx_throwing_conditional_cleanup(1, 1);
    if (result != 534) {
        fprintf(stderr,
                "throwing conditional cleanup returned %d, expected 534\n",
                result);
        return 5;
    }
    result = verified_cxx_throwing_conditional_cleanup(1, 0);
    if (result != 535) {
        fprintf(stderr,
                "nested throwing conditional cleanup returned %d, expected 535\n",
                result);
        return 6;
    }
    result = verified_cxx_local_reference_normal_cleanup();
    if (result != 55) {
        fprintf(stderr,
                "normal local reference cleanup returned %d, expected 55\n",
                result);
        return 7;
    }
    return rcc_test_main();
}

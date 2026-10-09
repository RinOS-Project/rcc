#include <stdio.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RINOS_TEST_ABI __attribute__((sysv_abi))
#else
#define RINOS_TEST_ABI
#endif

extern int rcc_test_main(void) RINOS_TEST_ABI;
extern int cxx_exception_cleanup_dependent_dmi(void) RINOS_TEST_ABI;
extern int cxx_exception_cleanup_completed_dmi_then_throw(void) RINOS_TEST_ABI;

int main(void) {
    int dependent_dmi_result = cxx_exception_cleanup_dependent_dmi();
    if (dependent_dmi_result != 1719) {
        fprintf(stderr, "dependent DMI cleanup returned %d, expected 1719\n",
                dependent_dmi_result);
        return 1;
    }
    int completed_dmi_result =
        cxx_exception_cleanup_completed_dmi_then_throw();
    if (completed_dmi_result != 1723) {
        fprintf(stderr, "completed DMI cleanup returned %d, expected 1723\n",
                completed_dmi_result);
        return 1;
    }
    return rcc_test_main();
}

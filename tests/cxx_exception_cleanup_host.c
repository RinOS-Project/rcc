#include <stdio.h>
#include <process.h>
#include <stdlib.h>
#include <stdint.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RINOS_TEST_ABI __attribute__((sysv_abi))
#else
#define RINOS_TEST_ABI
#endif

extern int rcc_test_main(void) RINOS_TEST_ABI;
extern int cxx_exception_cleanup_dependent_dmi(void) RINOS_TEST_ABI;
extern int cxx_exception_cleanup_completed_dmi_then_throw(void) RINOS_TEST_ABI;
extern int cxx_exception_cleanup_double_throw(void) RINOS_TEST_ABI;
void* RINOS_TEST_ABI rin_malloc(unsigned long size) {
    return malloc((size_t)size);
}
void RINOS_TEST_ABI rin_free(void* pointer) {
    free(pointer);
}

int main(int argc, char** argv) {
    intptr_t child_status;
    if (argc == 2 && argv[1][0] == '-' && argv[1][1] == '-') {
        (void)cxx_exception_cleanup_double_throw();
        return 0;
    }
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
    child_status = _spawnl(_P_WAIT, argv[0], argv[0], "--double-throw", NULL);
    if (child_status == 0) {
        fprintf(stderr, "double exception during cleanup was not terminated\n");
        return 1;
    }
    if (child_status == -1) {
        perror("failed to run double-exception cleanup test");
        return 1;
    }
    return rcc_test_main();
}

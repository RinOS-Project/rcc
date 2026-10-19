#include <stdio.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RINOS_TEST_ABI __attribute__((sysv_abi))
#else
#define RINOS_TEST_ABI
#endif

extern int verified_cxx_throwing_temporary_cleanup(int should_throw)
    RINOS_TEST_ABI;
extern int verified_cxx_throwing_argument_cleanup(int* cleanup_order)
    RINOS_TEST_ABI;
extern int verified_cxx_throwing_conditional_cleanup(
    int select_outer, int select_inner, int* cleanup_order)
    RINOS_TEST_ABI;
extern int verified_cxx_local_reference_normal_cleanup(void) RINOS_TEST_ABI;
extern int verified_cxx_throwing_initializer_cleanup(int should_throw)
    RINOS_TEST_ABI;
extern int verified_cxx_throwing_constructor_reference_cleanup(
    int should_throw) RINOS_TEST_ABI;
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
    int cleanup_order = 0;
    result = verified_cxx_throwing_argument_cleanup(&cleanup_order);
    if (result != 5303 || cleanup_order != 3) {
        fprintf(stderr,
                "throwing subobject reference cleanup returned %d/order %d, expected 5303/order 3\n",
                result, cleanup_order);
        return 3;
    }
    result = verified_cxx_throwing_conditional_cleanup(
        0, 1, &cleanup_order);
    if (result != 533 || cleanup_order != 3) {
        fprintf(stderr,
                "throwing conditional cleanup returned %d/order %d, expected 533/order 3\n",
                result, cleanup_order);
        return 4;
    }
    result = verified_cxx_throwing_conditional_cleanup(
        1, 1, &cleanup_order);
    if (result != 534 || cleanup_order != 4) {
        fprintf(stderr,
                "throwing conditional cleanup returned %d/order %d, expected 534/order 4\n",
                result, cleanup_order);
        return 5;
    }
    result = verified_cxx_throwing_conditional_cleanup(
        1, 0, &cleanup_order);
    if (result != 535 || cleanup_order != 5) {
        fprintf(stderr,
                "nested throwing conditional cleanup returned %d/order %d, expected 535/order 5\n",
                result, cleanup_order);
        return 6;
    }
    result = verified_cxx_local_reference_normal_cleanup();
    if (result != 65708) {
        fprintf(stderr,
                "normal nested/conversion/prvalue-receiver reference cleanup returned %d, expected 65708\n",
                result);
        return 7;
    }
    result = verified_cxx_throwing_initializer_cleanup(1);
    if (result != 560) {
        fprintf(stderr,
                "throwing initializer cleanup returned %d, expected 560\n",
                result);
        return 8;
    }
    result = verified_cxx_throwing_initializer_cleanup(0);
    if (result != 66) {
        fprintf(stderr,
                "successful initializer cleanup returned %d, expected 66\n",
                result);
        return 9;
    }
    result = verified_cxx_throwing_constructor_reference_cleanup(1);
    if (result != 59031) {
        fprintf(stderr,
                "throwing member constructor temporary returned %d, expected 59031\n",
                result);
        return 10;
    }
    result = verified_cxx_throwing_constructor_reference_cleanup(2);
    if (result != 58231) {
        fprintf(stderr,
                "throwing constructor body temporary returned %d, expected 58231\n",
                result);
        return 11;
    }
    result = verified_cxx_throwing_constructor_reference_cleanup(0);
    if (result != 2231) {
        fprintf(stderr,
                "successful constructor temporary returned %d, expected 2231\n",
                result);
        return 12;
    }
    result = verified_cxx_throwing_constructor_reference_cleanup(3);
    if (result != 2231) {
        fprintf(stderr,
                "implicitly completed constructor temporary returned %d, expected 2231\n",
                result);
        return 13;
    }
    return rcc_test_main();
}

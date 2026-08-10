/* Test preprocessor options */

#ifdef TEST_MACRO
int test_defined = 1;
#else
int test_defined = 0;
#endif

#ifdef __FREESTANDING__
int freestanding = 1;
#else
int freestanding = 0;
#endif

int main(void) {
    return test_defined + freestanding;
}

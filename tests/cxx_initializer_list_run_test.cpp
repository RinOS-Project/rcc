#include <stdio.h>

static int constructed_count;
static int destroyed_count;

#ifdef TEST_GCC_INITIALIZER_LIST
#define INITIALIZER_LIST_HOST_ABI
#else
#define INITIALIZER_LIST_HOST_ABI __attribute__((sysv_abi))
#endif

extern "C" INITIALIZER_LIST_HOST_ABI
void initializer_list_record_constructed(int value) {
    (void)value;
    ++constructed_count;
}

extern "C" INITIALIZER_LIST_HOST_ABI
void initializer_list_record_destroyed(int value) {
    (void)value;
    ++destroyed_count;
}

extern "C" INITIALIZER_LIST_HOST_ABI
void initializer_list_reset_counts(void) {
    constructed_count = 0;
    destroyed_count = 0;
}

extern "C" INITIALIZER_LIST_HOST_ABI
int initializer_list_counts_are(int constructed, int destroyed) {
    return constructed_count == constructed &&
           destroyed_count == destroyed;
}

#ifdef TEST_GCC_INITIALIZER_LIST
int gcc_generated_main(void);
#define generated_main gcc_generated_main
#else
extern "C" int rcc_generated_main(void);
#define generated_main rcc_generated_main
#endif

int main() {
    int result = generated_main();
    if (result != 0) {
        fprintf(stderr, "initializer_list regression failed: %d\n", result);
    }
    return result;
}

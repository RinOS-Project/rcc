#include <assert.h>
#include <stdio.h>

extern int rcc_generated_main(void);

int main(void)
{
    int result = rcc_generated_main();
    if (result != 0) fprintf(stderr, "RCC narrowing fixture returned %d\n", result);
    assert(result == 0);
    return 0;
}

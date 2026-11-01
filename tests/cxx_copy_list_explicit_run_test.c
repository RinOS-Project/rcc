#include <assert.h>

extern int rcc_generated_main(void);

int main(void) {
    assert(rcc_generated_main() == 0);
    return 0;
}

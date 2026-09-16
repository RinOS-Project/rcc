/* The generated C++ main is renamed so this host supplies the CRT entry. */

#include <stdlib.h>

extern int rcc_cxx_constexpr_main(void);

/* Keep the host link honest for constexpr functions that are also emitted as
 * callable C++ definitions.  These are real host allocator bridges, not
 * no-op substitutes; the constant-folded global itself never depends on them. */
void* rin_malloc(unsigned int size) {
    return malloc((size_t)size);
}

void rin_free(void* pointer) {
    free(pointer);
}

int main(void) {
    return rcc_cxx_constexpr_main();
}

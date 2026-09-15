#include <assert.h>
#include <stdlib.h>

int cxx_default_member_initializer(void);

void* rin_malloc(unsigned long size) {
    return malloc((size_t)size);
}

void rin_free(void* pointer) {
    free(pointer);
}

int main(void) {
    assert(cxx_default_member_initializer() == 0);
    return 0;
}

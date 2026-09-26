#include <assert.h>

extern int probe_cxx_utf8_literals(void);

int main(void)
{
    assert(probe_cxx_utf8_literals() == 1 + 1 + 'i' + 'S' + 3);
    return 0;
}

#include <assert.h>

extern int probe_cxx17_utf8_literals(void);

int main(void)
{
    assert(probe_cxx17_utf8_literals() == 4);
    return 0;
}

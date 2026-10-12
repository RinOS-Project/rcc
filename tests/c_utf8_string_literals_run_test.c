#include <assert.h>

extern int probe_c_utf8_string_literals(void);

int main(void)
{
    assert(probe_c_utf8_string_literals() == 4);
    return 0;
}

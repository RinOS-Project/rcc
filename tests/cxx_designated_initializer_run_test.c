#include <assert.h>

extern int probe_cxx_designated_initializer(void);

int main(void)
{
    assert(probe_cxx_designated_initializer() == 322);
    return 0;
}

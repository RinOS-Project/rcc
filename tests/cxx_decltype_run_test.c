#include <assert.h>

extern int probe_decltype(void);

int main(void)
{
    assert(probe_decltype() == 0);
    return 0;
}

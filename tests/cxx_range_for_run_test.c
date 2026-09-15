#include <assert.h>

extern int probe_function_pointer(void);
extern int probe_range_for(void);
extern int probe_range_for_explicit(void);

int main(void)
{
    assert(probe_function_pointer() == 42);
    assert(probe_range_for() == 6);
    assert(probe_range_for_explicit() == 9);
    return 0;
}

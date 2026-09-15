#include <assert.h>

extern int probe_function_pointer(void);
extern int probe_range_for(void);
extern int probe_range_for_explicit(void);
extern int probe_range_for_reference(void);
extern int probe_range_for_const_reference(void);

int main(void)
{
    assert(probe_function_pointer() == 42);
    assert(probe_range_for() == 6);
    assert(probe_range_for_explicit() == 9);
    assert(probe_range_for_reference() == 36);
    assert(probe_range_for_const_reference() == 21);
    return 0;
}

#include <assert.h>

extern int probe_lambda_pointer(void);

int main(void)
{
    assert(probe_lambda_pointer() == 42);
    return 0;
}

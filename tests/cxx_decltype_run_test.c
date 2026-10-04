#include <assert.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

extern int RIN_SYSV probe_decltype(void);

int main(void)
{
    assert(probe_decltype() == 0);
    return 0;
}

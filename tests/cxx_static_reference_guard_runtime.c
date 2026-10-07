#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int cxx_static_reference_retry_attempts;
int cxx_static_reference_guard_aborts;
int cxx_static_reference_guard_releases;
int cxx_static_reference_caught_value;

int RIN_SYSV __cxa_guard_acquire(unsigned long long* guard)
{
    unsigned char* state = (unsigned char*)guard;
    if (state[0] != 0u) return 0;
    if (state[1] != 0u) return 0;
    state[1] = 1u;
    return 1;
}

void RIN_SYSV __cxa_guard_release(unsigned long long* guard)
{
    unsigned char* state = (unsigned char*)guard;
    state[0] = 1u;
    state[1] = 0u;
    ++cxx_static_reference_guard_releases;
}

void RIN_SYSV __cxa_guard_abort(unsigned long long* guard)
{
    unsigned char* state = (unsigned char*)guard;
    state[1] = 0u;
    ++cxx_static_reference_guard_aborts;
}

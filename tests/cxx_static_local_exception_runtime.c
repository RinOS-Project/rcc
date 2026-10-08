#include <stddef.h>

#if defined(_WIN64)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

typedef void (RIN_SYSV *CxxStaticLocalCallback)(void*);

typedef struct CxxStaticLocalExit {
    CxxStaticLocalCallback callback;
    void* argument;
    void* dso;
    int active;
} CxxStaticLocalExit;

static CxxStaticLocalExit cxx_static_local_exits[8];
static size_t cxx_static_local_exit_count;
void* __dso_handle;

int cxx_static_local_attempts;
int cxx_static_local_guard_aborts;
int cxx_static_local_guard_releases;
int cxx_static_local_registrations;
int cxx_static_local_finalize_calls;
int cxx_static_local_dso_mismatches;
int cxx_static_local_registration_dso_mismatches;
int cxx_static_local_destructors;
int cxx_static_local_destruction_order;

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
    ++cxx_static_local_guard_releases;
}

void RIN_SYSV __cxa_guard_abort(unsigned long long* guard)
{
    unsigned char* state = (unsigned char*)guard;
    state[1] = 0u;
    ++cxx_static_local_guard_aborts;
}

int RIN_SYSV __cxa_atexit(CxxStaticLocalCallback callback,
                          void* argument, void* dso)
{
    CxxStaticLocalExit* entry;
    if (!callback || !argument ||
        cxx_static_local_exit_count >=
            sizeof(cxx_static_local_exits) / sizeof(cxx_static_local_exits[0])) {
        return -1;
    }
    if (dso != &__dso_handle)
        ++cxx_static_local_registration_dso_mismatches;
    entry = &cxx_static_local_exits[cxx_static_local_exit_count++];
    entry->callback = callback;
    entry->argument = argument;
    entry->dso = dso;
    entry->active = 1;
    ++cxx_static_local_registrations;
    return 0;
}

void RIN_SYSV __cxa_finalize(void* dso)
{
    size_t index = cxx_static_local_exit_count;
    ++cxx_static_local_finalize_calls;
    while (index > 0u) {
        CxxStaticLocalExit* entry = &cxx_static_local_exits[--index];
        if (!entry->active) continue;
        if (dso && entry->dso != dso) {
            ++cxx_static_local_dso_mismatches;
            continue;
        }
        entry->active = 0;
        entry->callback(entry->argument);
    }
}

void RIN_SYSV cxx_static_local_finalize(void)
{
    __cxa_finalize(&__dso_handle);
}

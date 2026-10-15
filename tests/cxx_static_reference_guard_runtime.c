#include <stddef.h>
#include <stdlib.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int cxx_static_reference_retry_attempts;
int cxx_static_reference_guard_aborts;
int cxx_static_reference_guard_releases;
int cxx_static_reference_caught_value;
int cxx_static_reference_class_attempts;
int cxx_static_reference_class_constructions;
int cxx_static_reference_class_destructions;
int cxx_static_reference_class_destruction_value;
int cxx_static_reference_atexit_registrations;
int cxx_static_reference_atexit_finalizations;

typedef void (RIN_SYSV *CxxStaticReferenceExitCallback)(void*);

typedef struct CxxStaticReferenceExitEntry {
    CxxStaticReferenceExitCallback callback;
    void* argument;
    void* dso;
    int active;
} CxxStaticReferenceExitEntry;

static CxxStaticReferenceExitEntry cxx_static_reference_exit_entries[8];
static size_t cxx_static_reference_exit_count;
void* __dso_handle;

int RIN_SYSV __cxa_atexit(CxxStaticReferenceExitCallback callback,
                          void* argument, void* dso)
{
    CxxStaticReferenceExitEntry* entry;
    if (!callback || cxx_static_reference_exit_count >=
            sizeof(cxx_static_reference_exit_entries) /
                sizeof(cxx_static_reference_exit_entries[0])) {
        return -1;
    }
    entry = &cxx_static_reference_exit_entries[
        cxx_static_reference_exit_count++];
    entry->callback = callback;
    entry->argument = argument;
    entry->dso = dso;
    entry->active = 1;
    ++cxx_static_reference_atexit_registrations;
    return 0;
}

void RIN_SYSV __cxa_finalize(void* dso)
{
    size_t index = cxx_static_reference_exit_count;
    while (index > 0u) {
        CxxStaticReferenceExitEntry* entry =
            &cxx_static_reference_exit_entries[--index];
        if (!entry->active || (dso && entry->dso != dso)) continue;
        entry->active = 0;
        entry->callback(entry->argument);
        ++cxx_static_reference_atexit_finalizations;
    }
}

void cxx_static_reference_retry_finalize(void)
{
    __cxa_finalize(&__dso_handle);
}

#ifdef RCC_STATIC_REFERENCE_RETRY_HOST
int RIN_SYSV _rcc_entry(void);

#if defined(__x86_64__)
void* RIN_SYSV rin_malloc(size_t size)
{
    return malloc(size);
}

void RIN_SYSV rin_free(void* allocation)
{
    free(allocation);
}
#endif

int main(void)
{
    return _rcc_entry();
}
#endif

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

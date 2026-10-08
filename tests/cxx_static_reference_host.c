#include <stddef.h>
#include <stdio.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

typedef void (RIN_SYSV *CxxExitCallback)(void*);

typedef struct CxxExitEntry {
    CxxExitCallback callback;
    void* argument;
    void* dso;
    int active;
} CxxExitEntry;

static CxxExitEntry cxx_exit_entries[16];
static size_t cxx_exit_count;
#if defined(__ELF__)
extern void* __dso_handle;
#else
void* __dso_handle;
#endif
extern int cxx_static_reference_events;
extern int cxx_static_reference_expected_events;
extern int RIN_SYSV rcc_generated_main(void);
extern void RIN_SYSV __rcc_global_init(void);
extern void RIN_SYSV __rcc_global_fini(void);

int RIN_SYSV __cxa_guard_acquire(long long* guard)
{
    unsigned char* bytes = (unsigned char*)guard;
    if (__atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE) != 0u) return 0;
    for (;;) {
        unsigned char expected = 0u;
        if (__atomic_compare_exchange_n(
                &bytes[1], &expected, 1u, 0,
                __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            if (__atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE) != 0u) {
                __atomic_store_n(&bytes[1], 0u, __ATOMIC_RELEASE);
                return 0;
            }
            return 1;
        }
        while (__atomic_load_n(&bytes[1], __ATOMIC_ACQUIRE) != 0u) {
        }
        if (__atomic_load_n(&bytes[0], __ATOMIC_ACQUIRE) != 0u) return 0;
    }
}

void RIN_SYSV __cxa_guard_release(long long* guard)
{
    unsigned char* bytes = (unsigned char*)guard;
    __atomic_store_n(&bytes[0], 1u, __ATOMIC_RELEASE);
    __atomic_store_n(&bytes[1], 0u, __ATOMIC_RELEASE);
}

void RIN_SYSV __cxa_guard_abort(long long* guard)
{
    __atomic_store_n(&((unsigned char*)guard)[1], 0u, __ATOMIC_RELEASE);
}

int RIN_SYSV __cxa_atexit(CxxExitCallback callback, void* argument, void* dso)
{
    if (!callback || !argument || cxx_exit_count ==
            sizeof(cxx_exit_entries) / sizeof(cxx_exit_entries[0])) {
        return -1;
    }
    cxx_exit_entries[cxx_exit_count].callback = callback;
    cxx_exit_entries[cxx_exit_count].argument = argument;
    cxx_exit_entries[cxx_exit_count].dso = dso;
    cxx_exit_entries[cxx_exit_count].active = 1;
    ++cxx_exit_count;
    return 0;
}

void RIN_SYSV __cxa_finalize(void* dso)
{
    size_t index = cxx_exit_count;
    while (index > 0u) {
        CxxExitEntry* entry = &cxx_exit_entries[--index];
        if (!entry->active || (dso && entry->dso != dso)) continue;
        entry->active = 0;
        entry->callback(entry->argument);
    }
}

int main(void)
{
    int result;
#if !defined(__ELF__)
    __rcc_global_init();
#endif
    result = rcc_generated_main();
    if (result != 0) {
        fprintf(stderr, "generated static-reference test failed: status %d\n",
                result);
        return result;
    }
    __cxa_finalize(&__dso_handle);
    __rcc_global_fini();
    if (cxx_static_reference_events != cxx_static_reference_expected_events) {
        fprintf(stderr, "static-reference events: expected %d, got %d\n",
                cxx_static_reference_expected_events,
                cxx_static_reference_events);
        return 90;
    }
    return 0;
}

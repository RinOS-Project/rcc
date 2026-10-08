#include <stddef.h>
#include <stdio.h>
#ifdef RCC_STATIC_REFERENCE_TLS_TEST
#include <pthread.h>
#include <stdint.h>
#endif

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

typedef struct CxxExceptionCleanupEntry {
    CxxExitCallback callback;
    void* object;
    int active;
} CxxExceptionCleanupEntry;

static CxxExitEntry cxx_exit_entries[16];
static size_t cxx_exit_count;
static CxxExceptionCleanupEntry cxx_exception_cleanups[16];
static size_t cxx_exception_cleanup_count;
static int cxx_exception_cleanup_error;
#if defined(__ELF__)
extern void* __dso_handle;
#else
void* __dso_handle;
#endif
extern int cxx_static_reference_events;
extern int cxx_static_reference_expected_events;
#ifdef RCC_STATIC_REFERENCE_TLS_TEST
extern int cxx_static_reference_tls_constructions;
extern int cxx_static_reference_tls_destructions;
extern int cxx_static_reference_tls_destruction_order;
extern int cxx_static_reference_tls_base_constructions;
extern int cxx_static_reference_tls_derived_constructions;
extern int cxx_static_reference_tls_base_destructions;
extern int cxx_static_reference_tls_derived_destructions;
extern int cxx_static_reference_tls_conversions;
extern int cxx_static_reference_tls_member_constructions;
extern int cxx_static_reference_tls_member_destructions;
extern int RIN_SYSV cxx_static_reference_tls_worker(int value);
#endif
extern int RIN_SYSV rcc_generated_main(void);
extern void RIN_SYSV __rcc_global_init(void);
extern void RIN_SYSV __rcc_global_fini(void);

#ifdef RCC_STATIC_REFERENCE_TLS_TEST
static void* cxx_static_reference_tls_thread(void* argument)
{
    int value = (int)(intptr_t)argument;
    int result = cxx_static_reference_tls_worker(value);
    return (void*)(intptr_t)result;
}

static int cxx_static_reference_run_tls_thread(int value)
{
    pthread_t thread;
    void* result = NULL;
    if (pthread_create(&thread, NULL, cxx_static_reference_tls_thread,
                       (void*)(intptr_t)value) != 0) {
        return -1;
    }
    if (pthread_join(thread, &result) != 0) return -2;
    return (int)(intptr_t)result;
}
#endif

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

void RIN_SYSV rin_cpp_exception_register_current_cleanup(
    CxxExitCallback callback, void* object)
{
    size_t index;
    if (!callback || !object) {
        cxx_exception_cleanup_error = 1;
        return;
    }
    for (index = 0u;
         index < sizeof(cxx_exception_cleanups) /
                     sizeof(cxx_exception_cleanups[0]);
         ++index) {
        CxxExceptionCleanupEntry* entry = &cxx_exception_cleanups[index];
        if (entry->active) continue;
        entry->callback = callback;
        entry->object = object;
        entry->active = 1;
        ++cxx_exception_cleanup_count;
        return;
    }
    cxx_exception_cleanup_error = 2;
}

void RIN_SYSV rin_cpp_exception_unregister_current_cleanup(
    CxxExitCallback callback, void* object)
{
    size_t index = sizeof(cxx_exception_cleanups) /
        sizeof(cxx_exception_cleanups[0]);
    while (index > 0u) {
        CxxExceptionCleanupEntry* entry = &cxx_exception_cleanups[--index];
        if (!entry->active || entry->callback != callback ||
            entry->object != object) {
            continue;
        }
        entry->active = 0;
        entry->callback = NULL;
        entry->object = NULL;
        --cxx_exception_cleanup_count;
        return;
    }
    cxx_exception_cleanup_error = 3;
}

int main(void)
{
    int result;
#ifdef RCC_STATIC_REFERENCE_TLS_TEST
    int tls_result;
#endif
#if !defined(__ELF__)
    __rcc_global_init();
#endif
    result = rcc_generated_main();
    if (result != 0) {
        fprintf(stderr, "generated static-reference test failed: status %d\n",
                result);
        return result;
    }
#ifdef RCC_STATIC_REFERENCE_TLS_TEST
    if (cxx_static_reference_tls_constructions != 0 ||
        cxx_static_reference_tls_destructions != 0 ||
        cxx_static_reference_tls_base_constructions != 0 ||
        cxx_static_reference_tls_derived_constructions != 0 ||
        cxx_static_reference_tls_base_destructions != 0 ||
        cxx_static_reference_tls_derived_destructions != 0 ||
        cxx_static_reference_tls_conversions != 0 ||
        cxx_static_reference_tls_member_constructions != 0 ||
        cxx_static_reference_tls_member_destructions != 0) return 92;
    tls_result = cxx_static_reference_run_tls_thread(3);
    if (tls_result != 0 || cxx_static_reference_tls_constructions != 1 ||
        cxx_static_reference_tls_destructions != 1 ||
        cxx_static_reference_tls_base_constructions != 3 ||
        cxx_static_reference_tls_derived_constructions != 3 ||
        cxx_static_reference_tls_base_destructions != 3 ||
        cxx_static_reference_tls_derived_destructions != 3 ||
        cxx_static_reference_tls_conversions != 2 ||
        cxx_static_reference_tls_member_constructions != 1 ||
        cxx_static_reference_tls_member_destructions != 1 ||
        cxx_static_reference_tls_destruction_order != 21521213) {
        fprintf(stderr,
                "TLS reference first-thread state: worker %d, direct %d/%d, "
                "base %d/%d, derived %d/%d, conversions %d, order %d\n",
                tls_result, cxx_static_reference_tls_constructions,
                cxx_static_reference_tls_destructions,
                cxx_static_reference_tls_base_constructions,
                cxx_static_reference_tls_base_destructions,
                cxx_static_reference_tls_derived_constructions,
                cxx_static_reference_tls_derived_destructions,
                cxx_static_reference_tls_conversions,
                cxx_static_reference_tls_destruction_order);
        return 93;
    }
    cxx_static_reference_tls_destruction_order = 0;
    tls_result = cxx_static_reference_run_tls_thread(4);
    if (tls_result != 0 || cxx_static_reference_tls_constructions != 2 ||
        cxx_static_reference_tls_destructions != 2 ||
        cxx_static_reference_tls_base_constructions != 6 ||
        cxx_static_reference_tls_derived_constructions != 6 ||
        cxx_static_reference_tls_base_destructions != 6 ||
        cxx_static_reference_tls_derived_destructions != 6 ||
        cxx_static_reference_tls_conversions != 4 ||
        cxx_static_reference_tls_member_constructions != 2 ||
        cxx_static_reference_tls_member_destructions != 2 ||
        cxx_static_reference_tls_destruction_order != 21521214) {
        fprintf(stderr,
                "TLS reference second-thread state: worker %d, direct %d/%d, "
                "base %d/%d, derived %d/%d, conversions %d, order %d\n",
                tls_result, cxx_static_reference_tls_constructions,
                cxx_static_reference_tls_destructions,
                cxx_static_reference_tls_base_constructions,
                cxx_static_reference_tls_base_destructions,
                cxx_static_reference_tls_derived_constructions,
                cxx_static_reference_tls_derived_destructions,
                cxx_static_reference_tls_conversions,
                cxx_static_reference_tls_destruction_order);
        return 94;
    }
#endif
    if (cxx_exception_cleanup_error || cxx_exception_cleanup_count != 0u) {
        fprintf(stderr,
                "static-reference exception cleanup state: error %d, active %zu\n",
                cxx_exception_cleanup_error, cxx_exception_cleanup_count);
        return 91;
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

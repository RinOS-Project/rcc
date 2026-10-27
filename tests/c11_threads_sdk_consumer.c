/* SPDX-License-Identifier: MIT */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error "C11 threads require a C11 or newer language mode"
#endif
#if defined(__STDC_NO_THREADS__)
#error "The RinOS SDK provides the C11 threads API"
#endif

#include <threads.h>

static once_flag g_once = ONCE_FLAG_INIT;
static thread_local int g_thread_local;

static void initialize_once(void)
{
}

static int start_thread(void* argument)
{
    g_thread_local = (int)(intptr_t)argument;
    return g_thread_local;
}

static void exit_thread(void) { thrd_exit(0); }

int c11_threads_sdk_consumer(void)
{
    mtx_t mutex;
    cnd_t condition;
    tss_t key;
    thrd_t thread;
    struct timespec deadline = {0, 0};
    struct timespec remaining;
    (void)mtx_init(&mutex, mtx_timed | mtx_recursive);
    (void)mtx_lock(&mutex);
    (void)mtx_timedlock(&mutex, &deadline);
    (void)mtx_trylock(&mutex);
    (void)mtx_unlock(&mutex);
    mtx_destroy(&mutex);
    (void)cnd_init(&condition);
    (void)cnd_wait(&condition, &mutex);
    (void)cnd_timedwait(&condition, &mutex, &deadline);
    (void)cnd_signal(&condition);
    (void)cnd_broadcast(&condition);
    cnd_destroy(&condition);
    (void)tss_create(&key, NULL);
    (void)tss_set(key, NULL);
    (void)tss_get(key);
    tss_delete(key);
    call_once(&g_once, initialize_once);
    (void)thrd_create(&thread, start_thread, NULL);
    (void)thrd_sleep(&deadline, &remaining);
    (void)thrd_current();
    (void)thrd_equal(thread, thrd_current());
    (void)thrd_join(thread, NULL);
    (void)thrd_detach(thread);
    thrd_yield();
    exit_thread();
    return thrd_success;
}

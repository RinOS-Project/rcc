#include "../include/atomic"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

struct HostWaitState {
    volatile uint32_t* address;
    pthread_cond_t condition;
    unsigned waiters;
    bool initialized;
};

static pthread_mutex_t g_host_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
static HostWaitState g_host_wait_states[64] = {};
static unsigned g_host_wait_calls;

static HostWaitState* find_host_wait_state(
    volatile uint32_t* address, bool create) {
    HostWaitState* empty = nullptr;
    for (unsigned i = 0; i < 64u; ++i) {
        HostWaitState* state = &g_host_wait_states[i];
        if (state->address == address) return state;
        if (!state->address && !empty) empty = state;
    }
    if (!create || !empty) return nullptr;
    if (!empty->initialized) {
        if (pthread_cond_init(&empty->condition, nullptr) != 0) return nullptr;
        empty->initialized = true;
    }
    empty->address = address;
    return empty;
}

extern "C" int rin_atomic_wait_test_host_wait(
    volatile uint32_t* address, uint32_t expected,
    const struct timespec* timeout) {
    pthread_mutex_lock(&g_host_wait_mutex);
    HostWaitState* state = find_host_wait_state(address, true);
    if (!state) {
        pthread_mutex_unlock(&g_host_wait_mutex);
        return -1;
    }
    ++state->waiters;
    ++g_host_wait_calls;

    while (__atomic_load_n(address, __ATOMIC_ACQUIRE) == expected) {
        int wait_result;
        if (timeout) {
            struct timespec deadline;
            timespec_get(&deadline, TIME_UTC);
            deadline.tv_sec += timeout->tv_sec;
            deadline.tv_nsec += timeout->tv_nsec;
            if (deadline.tv_nsec >= 1000000000L) {
                ++deadline.tv_sec;
                deadline.tv_nsec -= 1000000000L;
            }
            wait_result = pthread_cond_timedwait(
                &state->condition, &g_host_wait_mutex, &deadline);
        } else {
            wait_result = pthread_cond_wait(
                &state->condition, &g_host_wait_mutex);
        }
        if (wait_result != 0) break;
    }

    if (--state->waiters == 0u) state->address = nullptr;
    pthread_mutex_unlock(&g_host_wait_mutex);
    return 0;
}

extern "C" int rin_atomic_wait_test_host_wake_one(
    volatile uint32_t* address) {
    pthread_mutex_lock(&g_host_wait_mutex);
    HostWaitState* state = find_host_wait_state(address, false);
    if (state) pthread_cond_signal(&state->condition);
    pthread_mutex_unlock(&g_host_wait_mutex);
    return state ? 1 : 0;
}

extern "C" int rin_atomic_wait_test_host_wake_all(
    volatile uint32_t* address) {
    pthread_mutex_lock(&g_host_wait_mutex);
    HostWaitState* state = find_host_wait_state(address, false);
    const int waiters = state ? static_cast<int>(state->waiters) : 0;
    if (state) pthread_cond_broadcast(&state->condition);
    pthread_mutex_unlock(&g_host_wait_mutex);
    return waiters;
}

struct WaitTask {
    std::atomic<unsigned>* object;
    unsigned old_value;
    volatile unsigned* completed;
};

static void* wait_for_change(void* context) {
    WaitTask* task = static_cast<WaitTask*>(context);
    task->object->wait(task->old_value, std::memory_order_acquire);
    __atomic_add_fetch(task->completed, 1u, __ATOMIC_RELEASE);
    return nullptr;
}

static unsigned host_wait_call_count() {
    pthread_mutex_lock(&g_host_wait_mutex);
    const unsigned result = g_host_wait_calls;
    pthread_mutex_unlock(&g_host_wait_mutex);
    return result;
}

static unsigned completed_count(const volatile unsigned* completed) {
    return __atomic_load_n(completed, __ATOMIC_ACQUIRE);
}

static bool wait_for_host_wait_count(unsigned target) {
    for (unsigned attempt = 0; attempt < 3000u; ++attempt) {
        if (host_wait_call_count() >= target) return true;
        const struct timespec delay = {0, 1000000L};
        nanosleep(&delay, nullptr);
    }
    return host_wait_call_count() >= target;
}

static bool wait_for_completion(const volatile unsigned* completed,
                                unsigned target) {
    for (unsigned attempt = 0; attempt < 3000u; ++attempt) {
        if (completed_count(completed) >= target) return true;
        const struct timespec delay = {0, 1000000L};
        nanosleep(&delay, nullptr);
    }
    return completed_count(completed) >= target;
}

static bool run_notify_one_and_all(void) {
    std::atomic<unsigned> value(0u);
    volatile unsigned completed = 0u;
    pthread_t threads[2];
    WaitTask tasks[2] = {{&value, 0u, &completed},
                         {&value, 0u, &completed}};
    const unsigned initial_calls = host_wait_call_count();

    if (pthread_create(&threads[0], nullptr, wait_for_change, &tasks[0]) != 0)
        return false;
    if (pthread_create(&threads[1], nullptr, wait_for_change, &tasks[1]) != 0) {
        value.store(1u, std::memory_order_release);
        value.notify_all();
        pthread_join(threads[0], nullptr);
        return false;
    }

    if (!wait_for_host_wait_count(initial_calls + 2u)) {
        value.store(1u, std::memory_order_release);
        value.notify_all();
        pthread_join(threads[0], nullptr);
        pthread_join(threads[1], nullptr);
        return false;
    }

    value.store(1u, std::memory_order_release);
    value.notify_one();
    const bool one_woke = wait_for_completion(&completed, 1u);
    value.notify_all();
    pthread_join(threads[0], nullptr);
    pthread_join(threads[1], nullptr);
    return one_woke && completed_count(&completed) == 2u;
}

static bool run_unchanged_value_recheck(void) {
    std::atomic<unsigned> value(7u);
    volatile unsigned completed = 0u;
    pthread_t thread;
    WaitTask task = {&value, 7u, &completed};
    const unsigned initial_calls = host_wait_call_count();
    if (pthread_create(&thread, nullptr, wait_for_change, &task) != 0)
        return false;

    if (!wait_for_host_wait_count(initial_calls + 1u)) {
        value.store(8u, std::memory_order_release);
        value.notify_all();
        pthread_join(thread, nullptr);
        return false;
    }
    value.notify_one();
    const bool reparked = wait_for_host_wait_count(initial_calls + 2u);
    const bool stayed_waiting = completed_count(&completed) == 0u;
    value.store(8u, std::memory_order_release);
    value.notify_one();
    pthread_join(thread, nullptr);
    return reparked && stayed_waiting && completed_count(&completed) == 1u;
}

int main() {
    if (!run_notify_one_and_all()) {
        puts("atomic wait wrapper test: notify_one/notify_all failed");
        return 1;
    }
    if (!run_unchanged_value_recheck()) {
        puts("atomic wait wrapper test: unchanged value escaped wait");
        return 1;
    }
    puts("atomic wait wrapper runtime checks passed");
    return 0;
}

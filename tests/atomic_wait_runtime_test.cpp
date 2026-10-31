#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

struct HostWaitState {
    std::condition_variable condition;
    unsigned waiters = 0;
};

static std::mutex g_host_wait_mutex;
static std::unordered_map<uintptr_t, std::shared_ptr<HostWaitState>>
    g_host_wait_states;
static std::atomic<unsigned> g_host_wait_calls{0};

static uintptr_t host_wait_key(const volatile uint32_t* address) {
    return reinterpret_cast<uintptr_t>(address);
}

extern "C" int rin_atomic_wait_test_host_wait(
    volatile uint32_t* address, uint32_t expected,
    const struct timespec* timeout) {
    const uintptr_t key = host_wait_key(address);
    std::unique_lock<std::mutex> lock(g_host_wait_mutex);
    auto& entry = g_host_wait_states[key];
    if (!entry) entry = std::make_shared<HostWaitState>();
    const std::shared_ptr<HostWaitState> state = entry;
    ++state->waiters;
    g_host_wait_calls.fetch_add(1u, std::memory_order_relaxed);

    const auto ready = [address, expected] {
        return __atomic_load_n(address, __ATOMIC_ACQUIRE) != expected;
    };
    if (timeout) {
        const auto duration = std::chrono::seconds(timeout->tv_sec) +
            std::chrono::nanoseconds(timeout->tv_nsec);
        state->condition.wait_for(lock, duration, ready);
    } else {
        state->condition.wait(lock, ready);
    }

    if (--state->waiters == 0u) {
        const auto found = g_host_wait_states.find(key);
        if (found != g_host_wait_states.end() && found->second == state)
            g_host_wait_states.erase(found);
    }
    return 0;
}

extern "C" int rin_atomic_wait_test_host_wake_one(
    volatile uint32_t* address) {
    std::lock_guard<std::mutex> lock(g_host_wait_mutex);
    const auto found = g_host_wait_states.find(host_wait_key(address));
    if (found == g_host_wait_states.end()) return 0;
    found->second->condition.notify_one();
    return 1;
}

extern "C" int rin_atomic_wait_test_host_wake_all(
    volatile uint32_t* address) {
    std::lock_guard<std::mutex> lock(g_host_wait_mutex);
    const auto found = g_host_wait_states.find(host_wait_key(address));
    if (found == g_host_wait_states.end()) return 0;
    found->second->condition.notify_all();
    return static_cast<int>(found->second->waiters);
}

extern "C" {
unsigned long long __rin_atomic_wait_prepare_v1(void);
void __rin_atomic_wait_park_v1(const volatile void*, size_t,
                               unsigned long long);
void __rin_atomic_notify_v1(const volatile void*, size_t, int);
}

static bool value_matches_u32(const volatile void* object,
                              uint32_t expected, int memory_order) {
    uint32_t observed;
    __atomic_load(static_cast<const volatile uint32_t*>(object), &observed,
                  memory_order);
    return observed == expected;
}

struct WaitTask {
    volatile uint32_t* object;
    uint32_t expected;
    std::atomic<unsigned>* completed;
};

static void wait_task(WaitTask* task) {
    for (;;) {
        if (!value_matches_u32(task->object, task->expected,
                               __ATOMIC_ACQUIRE))
            break;
        const unsigned long long generation =
            __rin_atomic_wait_prepare_v1();
        if (!value_matches_u32(task->object, task->expected,
                               __ATOMIC_ACQUIRE))
            continue;
        __rin_atomic_wait_park_v1(task->object, sizeof(uint32_t), generation);
    }
    task->completed->fetch_add(1u, std::memory_order_release);
}

template<class Predicate>
static bool wait_until(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return predicate();
}

static bool wait_for_host_waits(unsigned target) {
    return wait_until([target] {
        return g_host_wait_calls.load(std::memory_order_acquire) >= target;
    });
}

int main() {
    volatile uint32_t value = 0u;
    std::atomic<unsigned> completed{0u};
    WaitTask first{&value, 0u, &completed};
    WaitTask second{&value, 0u, &completed};
    const unsigned initial_wait_calls =
        g_host_wait_calls.load(std::memory_order_relaxed);
    std::thread first_thread(wait_task, &first);
    std::thread second_thread(wait_task, &second);

    if (!wait_for_host_waits(initial_wait_calls + 2u)) {
        __atomic_store_n(&value, 1u, __ATOMIC_RELEASE);
        __rin_atomic_notify_v1(&value, sizeof(value), 1);
        first_thread.join();
        second_thread.join();
        std::puts("atomic wait test: both waiters did not block");
        return 1;
    }

    __atomic_store_n(&value, 1u, __ATOMIC_RELEASE);
    __rin_atomic_notify_v1(&value, sizeof(value), 0);
    if (!wait_until([&completed] {
            return completed.load(std::memory_order_acquire) == 1u;
        })) {
        __rin_atomic_notify_v1(&value, sizeof(value), 1);
        first_thread.join();
        second_thread.join();
        std::puts("atomic wait test: notify_one did not release a waiter");
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const bool notify_one_released_only_one =
        completed.load(std::memory_order_acquire) == 1u;
    __rin_atomic_notify_v1(&value, sizeof(value), 1);
    first_thread.join();
    second_thread.join();
    if (!notify_one_released_only_one ||
        completed.load(std::memory_order_acquire) != 2u) {
        std::puts("atomic wait test: notify_one/notify_all waiter counts differ");
        return 1;
    }

    volatile uint32_t unchanged_value = 7u;
    completed.store(0u, std::memory_order_relaxed);
    WaitTask spurious{&unchanged_value, 7u, &completed};
    const unsigned before_spurious =
        g_host_wait_calls.load(std::memory_order_relaxed);
    std::thread spurious_thread(wait_task, &spurious);
    if (!wait_for_host_waits(before_spurious + 1u)) {
        __atomic_store_n(&unchanged_value, 8u, __ATOMIC_RELEASE);
        __rin_atomic_notify_v1(&unchanged_value, sizeof(unchanged_value), 1);
        spurious_thread.join();
        std::puts("atomic wait test: spurious waiter did not block");
        return 1;
    }
    __rin_atomic_notify_v1(&unchanged_value, sizeof(unchanged_value), 0);
    if (!wait_for_host_waits(before_spurious + 2u) ||
        completed.load(std::memory_order_acquire) != 0u) {
        __atomic_store_n(&unchanged_value, 8u, __ATOMIC_RELEASE);
        __rin_atomic_notify_v1(&unchanged_value, sizeof(unchanged_value), 1);
        spurious_thread.join();
        std::puts("atomic wait test: unchanged value escaped the wait loop");
        return 1;
    }
    __atomic_store_n(&unchanged_value, 8u, __ATOMIC_RELEASE);
    __rin_atomic_notify_v1(&unchanged_value, sizeof(unchanged_value), 1);
    spurious_thread.join();
    if (completed.load(std::memory_order_acquire) != 1u) {
        std::puts("atomic wait test: changed value did not release waiter");
        return 1;
    }

    std::puts("atomic wait runtime checks passed");
    return 0;
}

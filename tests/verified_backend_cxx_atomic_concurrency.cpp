#include <atomic>

struct VerifiedAtomicCounterPair {
    unsigned int operations;
    unsigned int checksum;
};

static_assert(sizeof(VerifiedAtomicCounterPair) == 8);

static std::atomic<VerifiedAtomicCounterPair> verified_atomic_counter_pair{
    VerifiedAtomicCounterPair{0u, 0u}
};
static std::atomic<double> verified_atomic_double{0.0};

extern "C" int verified_atomic_lock_free_query(
    unsigned long size, const void* object)
{
    return __atomic_is_lock_free(size, object);
}

extern "C" int verified_atomic_always_lock_free()
{
    unsigned int object = 0u;
    return __atomic_always_lock_free(sizeof(object), &object);
}

extern "C" int verified_atomic_double_store_load()
{
    verified_atomic_double.store(1.5, std::memory_order_release);
    if (verified_atomic_double.load(std::memory_order_acquire) != 1.5) {
        return 1;
    }
    if (verified_atomic_double.exchange(
            2.5, std::memory_order_acq_rel) != 1.5) {
        return 2;
    }
    double expected = 2.5;
    if (!verified_atomic_double.compare_exchange_strong(
            expected, 3.5, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return 3;
    }
    return verified_atomic_double.load(std::memory_order_acquire) == 3.5
        ? 0 : 4;
}

extern "C" int verified_atomic_ref_char()
{
    char value = 5;
    std::atomic_ref<char> reference(value);
    if (!reference.is_lock_free()) {
        return 1;
    }
    if (reference.load(std::memory_order_relaxed) != 5) {
        return 2;
    }
    reference.store(9, std::memory_order_release);
    if (reference.exchange(12, std::memory_order_acq_rel) != 9) {
        return 3;
    }
    return reference.load(std::memory_order_acquire) == 12 && value == 12
        ? 0 : 4;
}

extern "C" void verified_atomic_pair_reset()
{
    verified_atomic_counter_pair.store(
        VerifiedAtomicCounterPair{0u, 0u}, std::memory_order_release);
}

extern "C" void verified_atomic_pair_increment(unsigned int iterations)
{
    for (unsigned int index = 0u; index < iterations; ++index) {
        VerifiedAtomicCounterPair expected =
            verified_atomic_counter_pair.load(std::memory_order_relaxed);
        for (;;) {
            const VerifiedAtomicCounterPair desired = {
                expected.operations + 1u,
                expected.checksum + 7u
            };
            if (verified_atomic_counter_pair.compare_exchange_weak(
                    expected, desired, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                break;
            }
        }
    }
}

extern "C" unsigned long long verified_atomic_pair_read()
{
    const VerifiedAtomicCounterPair value =
        verified_atomic_counter_pair.load(std::memory_order_acquire);
    return ((unsigned long long)value.operations << 32) | value.checksum;
}

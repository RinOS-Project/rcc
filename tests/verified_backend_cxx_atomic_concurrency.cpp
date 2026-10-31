#include <atomic>

struct VerifiedAtomicCounterPair {
    unsigned int operations;
    unsigned int checksum;
};

static_assert(sizeof(VerifiedAtomicCounterPair) == 8);

static std::atomic<VerifiedAtomicCounterPair> verified_atomic_counter_pair{
    VerifiedAtomicCounterPair{0u, 0u}
};

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

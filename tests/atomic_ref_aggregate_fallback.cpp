#include <atomic>

struct AtomicRefAggregateFallback {
    unsigned first;
    unsigned second;
};

union AtomicRefUnionFallback {
    unsigned value;
    unsigned char bytes[4];
};

struct AtomicRefWordAggregateFallback {
    unsigned value;
};

struct AtomicDefaultMemberAggregateFallback {
    unsigned first = 3u;
    unsigned second = 5u;
};

struct AtomicZeroDefaultAggregateFallback {
    unsigned zero{};
    unsigned initialized = 7u;
};

union AtomicDefaultMemberUnionFallback {
    unsigned integer = 11u;
    float floating;
};

union AtomicBraceDefaultMemberUnionFallback {
    unsigned integer{13u};
    float floating;
};

int main() {
    alignas(8) AtomicRefAggregateFallback storage{2u, 3u};
    std::atomic_ref<AtomicRefAggregateFallback> value(storage);

    AtomicRefAggregateFallback observed = value.load();
    if (observed.first != 2u || observed.second != 3u) return 1;

    value.store(AtomicRefAggregateFallback{5u, 8u},
                std::memory_order_release);
    observed = value.exchange(AtomicRefAggregateFallback{13u, 21u});
    if (observed.first != 5u || observed.second != 8u) return 2;

    AtomicRefAggregateFallback expected{13u, 21u};
    if (!value.compare_exchange_strong(
            expected, AtomicRefAggregateFallback{34u, 55u},
            std::memory_order_acq_rel, std::memory_order_acquire))
        return 3;

    expected = AtomicRefAggregateFallback{1u, 1u};
    if (value.compare_exchange_weak(
            expected, AtomicRefAggregateFallback{89u, 144u},
            std::memory_order_relaxed) ||
        expected.first != 34u || expected.second != 55u)
        return 4;

    alignas(4) AtomicRefUnionFallback union_storage{3u};
    std::atomic_ref<AtomicRefUnionFallback> union_value(union_storage);
    if (union_value.load().value != 3u) return 5;
    union_value.store(AtomicRefUnionFallback{5u});
    if (union_value.exchange(AtomicRefUnionFallback{7u}).value != 5u)
        return 6;
    AtomicRefUnionFallback union_expected{7u};
    if (!union_value.compare_exchange_strong(
            union_expected, AtomicRefUnionFallback{9u},
            std::memory_order_acq_rel, std::memory_order_acquire))
        return 7;
    union_expected = AtomicRefUnionFallback{11u};
    if (union_value.compare_exchange_weak(
            union_expected, AtomicRefUnionFallback{13u},
            std::memory_order_relaxed) || union_expected.value != 9u)
        return 8;

    alignas(4) AtomicRefWordAggregateFallback word_storage{7u};
    std::atomic_ref<AtomicRefWordAggregateFallback> word_value(word_storage);
    if (word_value.exchange(AtomicRefWordAggregateFallback{9u}).value != 7u ||
        word_storage.value != 9u)
        return 9;
    AtomicRefWordAggregateFallback word_expected{9u};
    if (!word_value.compare_exchange_strong(
            word_expected, AtomicRefWordAggregateFallback{11u},
            std::memory_order_acq_rel, std::memory_order_acquire) ||
        word_value.load().value != 11u)
        return 10;

    std::atomic<AtomicRefAggregateFallback> aggregate_atomic{
        AtomicRefAggregateFallback{2u, 3u}};
    observed = aggregate_atomic.load();
    if (observed.first != 2u || observed.second != 3u) return 11;
    aggregate_atomic.store(AtomicRefAggregateFallback{5u, 8u},
                           std::memory_order_release);
    observed = aggregate_atomic.exchange(
        AtomicRefAggregateFallback{13u, 21u});
    if (observed.first != 5u || observed.second != 8u) return 12;
    expected = AtomicRefAggregateFallback{13u, 21u};
    if (!aggregate_atomic.compare_exchange_strong(
            expected, AtomicRefAggregateFallback{34u, 55u},
            std::memory_order_acq_rel, std::memory_order_acquire))
        return 13;

    AtomicRefUnionFallback union_initial{17u};
    std::atomic<AtomicRefUnionFallback> union_atomic{union_initial};
    if (union_atomic.load().value != 17u) return 14;
    union_atomic.store(AtomicRefUnionFallback{19u},
                       std::memory_order_release);
    if (union_atomic.exchange(AtomicRefUnionFallback{23u}).value != 19u)
        return 15;
    AtomicRefUnionFallback atomic_union_expected{23u};
    if (!union_atomic.compare_exchange_strong(
            atomic_union_expected, AtomicRefUnionFallback{29u}))
        return 16;
    atomic_union_expected = AtomicRefUnionFallback{31u};
    if (union_atomic.compare_exchange_weak(
            atomic_union_expected, AtomicRefUnionFallback{37u}) ||
        atomic_union_expected.value != 29u)
        return 17;

    std::atomic<AtomicDefaultMemberAggregateFallback> member_default_atomic;
    const AtomicDefaultMemberAggregateFallback member_default =
        member_default_atomic.load();
    if (member_default.first != 3u || member_default.second != 5u)
        return 18;
    std::atomic<AtomicZeroDefaultAggregateFallback> zero_default_atomic;
    const AtomicZeroDefaultAggregateFallback zero_default =
        zero_default_atomic.load();
    if (zero_default.zero != 0u || zero_default.initialized != 7u)
        return 19;
    std::atomic<AtomicDefaultMemberUnionFallback> union_default_atomic;
    if (union_default_atomic.load().integer != 11u) return 20;
    std::atomic<AtomicBraceDefaultMemberUnionFallback> brace_union_default;
    if (brace_union_default.load().integer != 13u) return 21;
    return 0;
}

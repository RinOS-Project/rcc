#include <atomic>

struct NonTrivialAtomicValue {
    NonTrivialAtomicValue() : value(0) {}
    NonTrivialAtomicValue(const NonTrivialAtomicValue& other)
        : value(other.value) {}
    unsigned value;
};

void invalid_atomic_type() {
    NonTrivialAtomicValue value{};
    std::atomic<NonTrivialAtomicValue> atomic_value{value};
    std::atomic_ref<NonTrivialAtomicValue> reference{value};
    const unsigned immutable_value = 0;
    std::atomic<const unsigned> invalid_const_atomic{immutable_value};
    (void)atomic_value;
    (void)reference;
    (void)invalid_const_atomic;
}

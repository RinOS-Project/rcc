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
    (void)atomic_value;
    (void)reference;
}

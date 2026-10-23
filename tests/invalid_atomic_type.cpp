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
    std::atomic<unsigned[2]> invalid_array_atomic;
    std::atomic<void> invalid_void_atomic;
    std::atomic<unsigned()> invalid_function_atomic;
    (void)atomic_value;
    (void)reference;
    (void)invalid_const_atomic;
    (void)invalid_array_atomic;
    (void)invalid_void_atomic;
    (void)invalid_function_atomic;
}

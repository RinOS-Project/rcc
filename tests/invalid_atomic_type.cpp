#include <atomic>

struct NonTrivialAtomicValue {
    NonTrivialAtomicValue() : value(0) {}
    NonTrivialAtomicValue(const NonTrivialAtomicValue& other)
        : value(other.value) {}
    unsigned value;
};

struct NonConstructibleAtomicValue {
    NonConstructibleAtomicValue() = default;
    NonConstructibleAtomicValue(const NonConstructibleAtomicValue&) = delete;
};

struct NonAssignableAtomicValue {
    NonAssignableAtomicValue() = default;
    NonAssignableAtomicValue& operator=(const NonAssignableAtomicValue&) =
        delete;
};

struct NonMoveConstructibleAtomicValue {
    NonMoveConstructibleAtomicValue() = default;
    NonMoveConstructibleAtomicValue(
        const NonMoveConstructibleAtomicValue&) = default;
    NonMoveConstructibleAtomicValue(NonMoveConstructibleAtomicValue&&) =
        delete;
};

struct NonMoveAssignableAtomicValue {
    NonMoveAssignableAtomicValue() = default;
    NonMoveAssignableAtomicValue(const NonMoveAssignableAtomicValue&) =
        default;
    NonMoveAssignableAtomicValue(NonMoveAssignableAtomicValue&&) = default;
    NonMoveAssignableAtomicValue& operator=(
        const NonMoveAssignableAtomicValue&) = default;
    NonMoveAssignableAtomicValue& operator=(NonMoveAssignableAtomicValue&&) =
        delete;
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
    std::atomic<NonConstructibleAtomicValue> invalid_nonconstructible_atomic;
    std::atomic<NonAssignableAtomicValue> invalid_nonassignable_atomic;
    std::atomic<NonMoveConstructibleAtomicValue>
        invalid_nonmove_constructible_atomic;
    std::atomic<NonMoveAssignableAtomicValue> invalid_nonmove_assignable_atomic;
    (void)atomic_value;
    (void)reference;
    (void)invalid_const_atomic;
    (void)invalid_array_atomic;
    (void)invalid_void_atomic;
    (void)invalid_function_atomic;
    (void)invalid_nonconstructible_atomic;
    (void)invalid_nonassignable_atomic;
    (void)invalid_nonmove_constructible_atomic;
    (void)invalid_nonmove_assignable_atomic;
}

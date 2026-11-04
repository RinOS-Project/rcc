struct AtomicOpsBase {};

union AtomicUnionValue {
    unsigned value;
    unsigned char bytes[4];
};

struct AtomicDefaultMemberValue {
    unsigned first = 3u;
    unsigned second = 5u;
};

struct AtomicZeroDefaultMemberValue {
    unsigned zero{};
    unsigned initialized = 7u;
};

union AtomicDefaultMemberUnionValue {
    unsigned integer = 11u;
    float floating;
};

union AtomicBraceDefaultMemberUnionValue {
    unsigned integer{13u};
    float floating;
};

#define DEFINE_ATOMIC_STORAGE(Name, ValueType) \
    struct Name : AtomicOpsBase { \
        volatile ValueType value_; \
        Name() noexcept : value_() {} \
        Name(ValueType desired) noexcept : value_(desired) {} \
    }

DEFINE_ATOMIC_STORAGE(AtomicUnionStorage, AtomicUnionValue);
DEFINE_ATOMIC_STORAGE(AtomicDefaultMemberStorage, AtomicDefaultMemberValue);
DEFINE_ATOMIC_STORAGE(AtomicZeroDefaultMemberStorage,
                      AtomicZeroDefaultMemberValue);
DEFINE_ATOMIC_STORAGE(AtomicDefaultMemberUnionStorage,
                      AtomicDefaultMemberUnionValue);
DEFINE_ATOMIC_STORAGE(AtomicBraceDefaultMemberUnionStorage,
                      AtomicBraceDefaultMemberUnionValue);

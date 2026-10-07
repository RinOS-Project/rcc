enum InvalidImplicitEnumeratorOverflow {
    ENUM_SIGNED_MAXIMUM = 9223372036854775807LL,
    ENUM_SIGNED_OVERFLOW
};

enum InvalidUnsignedEnumeratorOverflow {
    ENUM_UNSIGNED_OVERFLOW = 18446744073709551615ULL
};

enum class InvalidFixedUnsignedEnumeratorStorage : unsigned long long {
    ENUM_FIXED_UNSIGNED_OVERFLOW = 18446744073709551615ULL
};

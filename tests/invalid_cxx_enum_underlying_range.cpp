enum class UnsignedByteTooSmall : unsigned char {
    value = 256,
};

enum class NegativeUnsigned : unsigned {
    value = -1,
};

enum class SignedByteTooSmall : signed char {
    value = 128,
};

enum class DefaultIntTooSmall {
    value = 2147483648LL,
};

enum class ImplicitUnsignedOverflow : unsigned char {
    maximum = 255,
    overflow,
};

enum ImplicitSignedOverflow : signed char {
    maximum = 127,
    overflow,
};

enum class ImplicitUnsignedLongLongOverflow : unsigned long long {
    maximum = 18446744073709551615ULL,
    overflow,
};

enum class SignedLongLongTooSmall : long long {
    value = 18446744073709551615ULL,
};

enum InferredUnsignedLongLongOverflow {
    inferred_ull_maximum = 18446744073709551615ULL,
    inferred_ull_overflow,
};

enum InferredRangeHasNoIntegerType {
    inferred_negative = -1,
    inferred_unsigned_maximum = 18446744073709551615ULL,
};

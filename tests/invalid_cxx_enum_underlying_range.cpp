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

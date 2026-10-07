/* C++ scoped-enum declaration, qualification, and type checking coverage. */
enum class Color {
    red = 3,
    blue = 7,
};

enum class Mode : unsigned {
    cold = 1,
    hot = 2,
};

enum ArithmeticEnumerator {
    first = 1 + 2 * 3,
    second = first << 1,
    third = second == 14 ? first : 0,
};

enum class UnsignedByte : unsigned char {
    maximum = 255,
};

enum class SignedByte : signed char {
    minimum = -128,
    maximum = 127,
};

enum UnscopedWideUnsigned : unsigned long long {
    wide_value = 0x100000000ULL,
};
static_assert(wide_value == 0x100000000ULL,
              "unscoped wide enumerator value");

enum class FullUnsigned : unsigned long long {
    almost = 18446744073709551614ULL,
    maximum,
};

enum InferredInt {
    inferred_int_value = 17,
    inferred_int_next,
};

enum InferredNegative {
    inferred_negative_value = -1,
    inferred_negative_next,
};

enum InferredUnsignedInt {
    inferred_uint_value = 0xFFFFFFFFU,
};

enum InferredUnsignedProgression {
    inferred_unsigned_base = 0x80000000U,
    inferred_unsigned_next = inferred_unsigned_base + 1,
};

enum InferredAfterIntMax {
    inferred_int_maximum = 2147483647,
    inferred_int_overflow,
};

enum InferredWide {
    inferred_wide_value = 0x100000000ULL,
    inferred_wide_next,
};

enum InferredFullUnsigned {
    inferred_full_unsigned = 18446744073709551615ULL,
};

static_assert(static_cast<unsigned long long>(FullUnsigned::maximum) ==
              18446744073709551615ULL, "unsigned 64-bit enum progression");
static_assert(sizeof(InferredInt) == sizeof(int), "small enum underlying type");
static_assert(sizeof(InferredNegative) == sizeof(int),
              "negative enum selects a signed underlying type");
static_assert(inferred_negative_next == 0,
              "negative implicit enumerator progression");
static_assert(sizeof(InferredUnsignedInt) == sizeof(unsigned int),
              "unsigned int enum underlying type");
static_assert(sizeof(InferredUnsignedProgression) == sizeof(unsigned int),
              "enumerator expression retains its unsigned type");
static_assert(inferred_unsigned_next == 0x80000001U,
              "prior enumerator has its initializer type before enum close");
static_assert(sizeof(InferredAfterIntMax) == sizeof(unsigned int),
              "implicit enumerator widens after int maximum");
static_assert(inferred_int_overflow == 2147483648U,
              "implicit enumerator selects a type that holds its value");
static_assert(sizeof(InferredWide) == 8, "wide enum underlying type");
static_assert(inferred_wide_next == 0x100000001ULL,
              "wide implicit enumerator increment");
static_assert(sizeof(InferredFullUnsigned) == 8,
              "full-width unsigned enum underlying type");
static_assert(inferred_full_unsigned == 18446744073709551615ULL,
              "unfixed enum preserves ULLONG_MAX");

int cxx_enum_class_probe() {
    Color color = Color::blue;
    Mode mode = Mode::hot;
    if (color != Color::blue) return 1;
    if (mode != Mode::hot) return 2;
    if (first != 7) return 3;
    if (second != 14) return 4;
    if (third != 7) return 5;
    if (static_cast<unsigned>(UnsignedByte::maximum) != 255u) return 6;
    if (static_cast<int>(SignedByte::minimum) != -128) return 7;
    if (static_cast<int>(SignedByte::maximum) != 127) return 8;
    if (wide_value != 0x100000000ULL) return 9;
    if (static_cast<unsigned long long>(FullUnsigned::maximum) !=
        18446744073709551615ULL) return 10;
    if (inferred_unsigned_next != 0x80000001U) return 11;
    if (inferred_negative_next != 0) return 12;
    if (inferred_int_overflow != 2147483648U) return 13;
    if (inferred_wide_next != 0x100000001ULL) return 14;
    if (inferred_full_unsigned != 18446744073709551615ULL) return 15;
    return 0;
}

int main() {
    return cxx_enum_class_probe();
}

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentArrayBound {
    int elements[(Value == 18446744073709551615ULL) ? 3 : 1];
};

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentWrapArrayBound {
    int elements[((Value + 1ULL) == 0ULL) ? 1 : 2];
};

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentComparisonArrayBounds {
    int greater_than_zero[(Value > 0ULL) ? 2 : 1];
    int less_than_zero[(Value < 0ULL) ? 1 : 3];
    int equal_to_max[(Value == 18446744073709551615ULL) ? 4 : 1];
};

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentArithmeticArrayBounds {
    int multiply[((Value * 2ULL) == 18446744073709551614ULL) ? 2 : 1];
    int divide[((Value / 2ULL) == 9223372036854775807ULL) ? 3 : 1];
    int remainder[((Value % 3ULL) == 0ULL) ? 4 : 1];
};

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentBitwiseArrayBounds {
    int bit_and[((Value & 0xFFULL) == 0xFFULL) ? 2 : 1];
    int bit_or[((Value | 0ULL) == Value) ? 3 : 1];
    int bit_xor[((Value ^ Value) == 0ULL) ? 4 : 1];
    int bit_complement[(~Value == 0ULL) ? 5 : 1];
};

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentShiftArrayBounds {
    int left_shift[((Value << 1) == 18446744073709551614ULL) ? 6 : 1];
    int right_shift[((Value >> 63) == 1ULL) ? 7 : 1];
};

template <unsigned long long Value>
struct DirectUnsigned64TemplateArgumentNarrowingArrayBound {
    int elements[(static_cast<unsigned char>(Value) == 255U) ? 8 : 1];
};

static_assert(sizeof(DirectUnsigned64TemplateArgumentArrayBound<
                  18446744073709551615ULL>) == 3u * sizeof(int));
static_assert(sizeof(DirectUnsigned64TemplateArgumentWrapArrayBound<
                  18446744073709551615ULL>) == sizeof(int));
static_assert(sizeof(DirectUnsigned64TemplateArgumentComparisonArrayBounds<
                  18446744073709551615ULL>) == 9u * sizeof(int));
static_assert(sizeof(DirectUnsigned64TemplateArgumentArithmeticArrayBounds<
                  18446744073709551615ULL>) == 9u * sizeof(int));
static_assert(sizeof(DirectUnsigned64TemplateArgumentBitwiseArrayBounds<
                  18446744073709551615ULL>) == 14u * sizeof(int));
static_assert(sizeof(DirectUnsigned64TemplateArgumentShiftArrayBounds<
                  18446744073709551615ULL>) == 13u * sizeof(int));
static_assert(sizeof(DirectUnsigned64TemplateArgumentNarrowingArrayBound<
                  18446744073709551615ULL>) == 8u * sizeof(int));

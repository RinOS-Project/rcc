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

template <int Value>
struct DependentFloatingCastArrayBounds {
    int double_to_int[(static_cast<int>(Value * 2.5) == 5) ? 3 : 1];
    int float_to_int[(static_cast<int>(static_cast<float>(Value) * 1.5f) == 3)
                         ? 5 : 1];
    int negative_double_to_int[(static_cast<int>(Value * 1.5) == -4)
                                   ? 7 : 1];
    int double_to_bool[static_cast<bool>(Value * 0.5) ? 13 : 1];
};

static_assert(sizeof(DependentFloatingCastArrayBounds<2>) ==
              22u * sizeof(int));
static_assert(sizeof(DependentFloatingCastArrayBounds<-3>) ==
              22u * sizeof(int));

template <int Value>
struct DependentUnsignedFloatingCastArrayBound {
    int items[(static_cast<unsigned>(Value * 1.25) == 2u) ? 11 : 1];
};

static_assert(sizeof(DependentUnsignedFloatingCastArrayBound<2>) ==
              11u * sizeof(int));

#if defined(RCC_TEST_INVALID_FLOAT_TO_INTEGER_BOUND)
template <int Value>
struct InvalidDependentFloatingToIntegerBound {
    int items[static_cast<int>(Value * 1.0e20)];
};
static_assert(sizeof(InvalidDependentFloatingToIntegerBound<1>) > 0u);
#endif

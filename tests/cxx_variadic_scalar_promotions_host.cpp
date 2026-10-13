enum VariadicPromotedByte : unsigned char {
    variadic_promoted_byte_value = 13
};

enum class VariadicScopedWord : unsigned int {
    value = 17
};

enum class VariadicScopedLongLong : long long {
    value = 0x100000007LL
};

enum VariadicPromotedUShort : unsigned short {
    variadic_promoted_ushort_value = 60000
};

enum VariadicUnpromotedWord : unsigned int {
    variadic_unpromoted_word_value = 19
};

enum VariadicInferredUnsignedWord {
    variadic_inferred_unsigned_word_value = 0x80000005U
};

enum VariadicPromotedSChar : signed char {
    variadic_promoted_schar_value = -12
};

enum VariadicPromotedShort : short {
    variadic_promoted_short_value = 30000
};

enum VariadicFixedInt : int {
    variadic_fixed_int_value = 29
};

enum VariadicUnpromotedLongLong : long long {
    variadic_unpromoted_long_long_value = 0x100000003LL
};

enum VariadicUnpromotedUnsignedLongLong : unsigned long long {
    variadic_unpromoted_unsigned_long_long_value = 0x8000000000000005ULL
};

#if defined(RCC_VARIADIC_HOST_CALLEE)
int read_variadic_scalar_promotions(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...) {
    __builtin_va_list arguments;
    __builtin_va_start(arguments, marker5);
    double promoted_float = __builtin_va_arg(arguments, double);
    int promoted_bool = __builtin_va_arg(arguments, int);
    int promoted_signed_char = __builtin_va_arg(arguments, int);
    int promoted_unsigned_char = __builtin_va_arg(arguments, int);
    int promoted_short = __builtin_va_arg(arguments, int);
    int promoted_unsigned_short = __builtin_va_arg(arguments, int);
    int promoted_enum = __builtin_va_arg(arguments, int);
    VariadicScopedWord scoped_enum =
        __builtin_va_arg(arguments, VariadicScopedWord);
    VariadicScopedLongLong scoped_long_long_enum =
        __builtin_va_arg(arguments, VariadicScopedLongLong);
    int promoted_ushort_enum = __builtin_va_arg(arguments, int);
    unsigned int promoted_word_enum =
        __builtin_va_arg(arguments, unsigned int);
    unsigned int promoted_inferred_word_enum =
        __builtin_va_arg(arguments, unsigned int);
    int promoted_schar_enum = __builtin_va_arg(arguments, int);
    int promoted_short_enum = __builtin_va_arg(arguments, int);
    int fixed_int_enum = __builtin_va_arg(arguments, int);
    long long preserved_long_long_enum =
        __builtin_va_arg(arguments, long long);
    unsigned long long preserved_unsigned_long_long_enum =
        __builtin_va_arg(arguments, unsigned long long);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 +
           (promoted_float == 1.5 ? 1 : 0) + promoted_bool +
           promoted_signed_char + promoted_unsigned_char + promoted_short +
           promoted_unsigned_short + promoted_enum + (int)scoped_enum +
           (scoped_long_long_enum == VariadicScopedLongLong::value) +
           promoted_ushort_enum + (int)promoted_word_enum +
           (promoted_inferred_word_enum ==
            variadic_inferred_unsigned_word_value) +
           promoted_schar_enum + promoted_short_enum + fixed_int_enum +
           (preserved_long_long_enum ==
            variadic_unpromoted_long_long_value) +
           (preserved_unsigned_long_long_enum ==
            variadic_unpromoted_unsigned_long_long_value);
}
#else
int read_variadic_scalar_promotions(
    int marker0, int marker1, int marker2, int marker3, int marker4,
    int marker5, ...);

int main() {
    float value = 1.5f;
    bool boolean_value = true;
    signed char signed_char_value = -3;
    unsigned char unsigned_char_value = 250;
    short short_value = 7;
    unsigned short unsigned_short_value = 9;
    VariadicPromotedByte enum_value = variadic_promoted_byte_value;
    VariadicScopedWord scoped_enum_value = VariadicScopedWord::value;
    VariadicScopedLongLong scoped_long_long_enum_value =
        VariadicScopedLongLong::value;
    VariadicPromotedUShort ushort_enum_value =
        variadic_promoted_ushort_value;
    VariadicUnpromotedWord word_enum_value =
        variadic_unpromoted_word_value;
    VariadicInferredUnsignedWord inferred_word_enum_value =
        variadic_inferred_unsigned_word_value;
    VariadicPromotedSChar schar_enum_value =
        variadic_promoted_schar_value;
    VariadicPromotedShort short_enum_value =
        variadic_promoted_short_value;
    VariadicFixedInt fixed_int_enum_value = variadic_fixed_int_value;
    VariadicUnpromotedLongLong long_long_enum_value =
        variadic_unpromoted_long_long_value;
    VariadicUnpromotedUnsignedLongLong unsigned_long_long_enum_value =
        variadic_unpromoted_unsigned_long_long_value;
    int result = read_variadic_scalar_promotions(
        2, 0, 0, 0, 0, 0, value, boolean_value, signed_char_value,
        unsigned_char_value, short_value, unsigned_short_value, enum_value,
        scoped_enum_value, scoped_long_long_enum_value, ushort_enum_value,
        word_enum_value, inferred_word_enum_value, schar_enum_value,
        short_enum_value, fixed_int_enum_value,
        long_long_enum_value, unsigned_long_long_enum_value);
    return result == 90337 ? 0 : 1;
}
#endif

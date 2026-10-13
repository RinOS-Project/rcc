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

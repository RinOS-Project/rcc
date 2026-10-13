enum VariadicPromotedByte : unsigned char {
    variadic_promoted_byte_value = 13
};

enum class VariadicScopedWord : unsigned int {
    value = 17
};

enum VariadicPromotedUShort : unsigned short {
    variadic_promoted_ushort_value = 60000
};

enum VariadicUnpromotedWord : unsigned int {
    variadic_unpromoted_word_value = 19
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
    int promoted_ushort_enum = __builtin_va_arg(arguments, int);
    unsigned int promoted_word_enum =
        __builtin_va_arg(arguments, unsigned int);
    __builtin_va_end(arguments);

    return marker0 + marker1 + marker2 + marker3 + marker4 + marker5 +
           (promoted_float == 1.5 ? 1 : 0) + promoted_bool +
           promoted_signed_char + promoted_unsigned_char + promoted_short +
           promoted_unsigned_short + promoted_enum + (int)scoped_enum +
           promoted_ushort_enum + (int)promoted_word_enum;
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
    VariadicPromotedUShort ushort_enum_value =
        variadic_promoted_ushort_value;
    VariadicUnpromotedWord word_enum_value =
        variadic_unpromoted_word_value;
    int result = read_variadic_scalar_promotions(
        2, 0, 0, 0, 0, 0, value, boolean_value, signed_char_value,
        unsigned_char_value, short_value, unsigned_short_value, enum_value,
        scoped_enum_value, ushort_enum_value, word_enum_value);
    return result == 60316 ? 0 : 1;
}
#endif

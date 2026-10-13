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
        word_enum_value, inferred_word_enum_value, schar_enum_value, short_enum_value,
        fixed_int_enum_value,
        long_long_enum_value, unsigned_long_long_enum_value);
    return result == 90337 ? 0 : 1;
}

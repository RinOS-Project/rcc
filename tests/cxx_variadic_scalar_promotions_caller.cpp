enum VariadicPromotedByte : unsigned char {
    variadic_promoted_byte_value = 13
};

enum class VariadicScopedWord : unsigned int {
    value = 17
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
    int result = read_variadic_scalar_promotions(
        2, 0, 0, 0, 0, 0, value, boolean_value, signed_char_value,
        unsigned_char_value, short_value, unsigned_short_value, enum_value,
        scoped_enum_value);
    return result == 297 ? 0 : 1;
}

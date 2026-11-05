struct ListValues {
    int number;
    unsigned char byte;
    float fraction;
};

struct ListConstructor {
    int value;
    ListConstructor(int source) : value(source) {}
};

struct ExactIntegerConversion {
    constexpr operator int() const { return 42; }
};

struct FloatingConversion {
    constexpr operator double() const { return 2.5; }
};

enum ListNarrowingEnumValues {
    list_narrowing_bool_false = 0,
    list_narrowing_bool_true = 1,
    list_narrowing_byte_max = 255,
    list_narrowing_exact_float = 16777216
};

enum ListNarrowingUnsignedEnumValues : unsigned int {
    list_narrowing_unsigned_exact_float = 4026531840u
};

float list_narrowing_take_float(float value) {
    return value;
}

int list_narrowing_unselected_call_count;

int list_narrowing_unselected_call() {
    ++list_narrowing_unselected_call_count;
    return 128;
}

int list_narrowing_pick(int) {
    return 1;
}

int list_narrowing_pick(float) {
    return 2;
}

int main() {
    int runtime_narrow = 256;
    ListValues values{255u, 255u, 1.5};
    ListValues parenthesized(255u, runtime_narrow, 1.1);
    int scalar{255u};
    bool boolean{1};
    bool nullptr_boolean{nullptr};
    bool exact_enum_false{list_narrowing_bool_false};
    bool exact_enum_true{list_narrowing_bool_true};
    signed char exact_signed_max{127};
    signed char exact_signed_min{-128};
    unsigned char exact_unsigned_max{255};
    float exact_fraction{1.5};
    float rounded_fraction{1.1};
    float positive_constant_underflow{1e-50};
    float negative_constant_underflow{-1e-50};
    signed char selected_constant_signed{true ? 127 : 128};
    unsigned char selected_constant_unsigned{true ? 0 : -1};
    signed char unselected_runtime_branch{
        false ? list_narrowing_unselected_call() : 127};
    float selected_constant_float{true ? 1.0 : 1.0e100};
    float exact_integer{16777216};
    double exact_wide_integer{9007199254740992ULL};
    double exact_ull_boundary{18446744073709549568ULL};
    constexpr double exact_nonfinite_nan = __builtin_nan("");
    float exact_nonfinite_nan_float{exact_nonfinite_nan};
    unsigned char exact_enum_byte{list_narrowing_byte_max};
    float exact_enum_float{list_narrowing_exact_float};
    float exact_unsigned_enum_float{
        list_narrowing_unsigned_exact_float};
    float enum_float_argument =
        list_narrowing_take_float(list_narrowing_exact_float);
    ListConstructor constructed{255u};
    short exact_user_conversion{ExactIntegerConversion{}};
    float user_floating_conversion{FloatingConversion{}};
    if (values.number != scalar || values.byte != 255u ||
        values.fraction != 1.5f || exact_fraction != 1.5f) return 1;
    if (parenthesized.number != 255 || parenthesized.byte != 0 ||
        parenthesized.fraction != 1.1f) return 2;
    if (!boolean) return 3;
    if (nullptr_boolean || exact_enum_false || !exact_enum_true) return 22;
    if (exact_signed_max != 127 || exact_signed_min != -128 ||
        exact_unsigned_max != 255) return 16;
    if (rounded_fraction != 1.1f) return 4;
    if (positive_constant_underflow != 0.0f ||
        negative_constant_underflow != 0.0f) return 17;
    if (selected_constant_signed != 127 ||
        selected_constant_unsigned != 0 ||
        unselected_runtime_branch != 127 ||
        selected_constant_float != 1.0f ||
        list_narrowing_unselected_call_count != 0) return 18;
    if (exact_integer != 16777216.0f) return 5;
    if (exact_wide_integer != 9007199254740992.0) return 6;
    if (exact_ull_boundary != 18446744073709549568.0) return 15;
    if (!__builtin_isnan(exact_nonfinite_nan_float)) return 21;
    if (exact_enum_byte != 255u) return 10;
    if (exact_enum_float != 16777216.0f) return 11;
    if (exact_unsigned_enum_float != 4026531840.0f) return 12;
    if (enum_float_argument != 16777216.0f) return 13;
    if (list_narrowing_pick(list_narrowing_exact_float) != 1) return 14;
    if (constructed.value != 255) return 7;
    if (exact_user_conversion != 42) return 8;
    if (user_floating_conversion != 2.5f) return 9;
    return 0;
}

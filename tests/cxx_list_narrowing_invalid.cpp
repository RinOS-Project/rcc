struct AggregateValue {
    int value;
};

struct NestedValue {
    unsigned char byte;
};

struct NestedAggregate {
    NestedValue nested;
};

struct ConstructedValue {
    int value;
    ConstructedValue(int source) : value(source) {}
};

struct OutOfRangeIntegerConversion {
    constexpr operator int() const { return 40000; }
};

struct RuntimeIntegerConversion {
    operator int() const;
};

struct FloatingConversion {
    constexpr operator double() const { return 2.5; }
};

enum ListNarrowingEnumValues {
    list_narrowing_negative = -1,
    list_narrowing_inexact_float = 16777217
};

enum ListNarrowingUnsignedEnumValues : unsigned int {
    list_narrowing_unsigned_inexact_float = 4026531841u
};

int main() {
    int source = 1;
    int source_array[1] = {1};
    struct SignedWideBitfield { int value : 9; } signed_wide = {};
    struct UnsignedWideBitfield { unsigned int value : 9; } unsigned_wide = {};
    struct SignedNarrowBitfield { int value : 4; } signed_narrow = {};
    AggregateValue from_float{1.5};
    NestedAggregate nested_out_of_range{{256}};
    ConstructedValue constructor_from_float{1.5};
    unsigned char out_of_range{256};
    unsigned char array_out_of_range[1]{256};
    float inexact_integer{16777217};
    double inexact_ull_max{18446744073709551615ULL};
    double runtime_fraction = 1.5;
    float runtime_narrow{runtime_fraction};
    float floating_overflow{1e100};
    bool nonzero_integer{2};
    signed char signed_overflow{128};
    signed char signed_underflow{-129};
    unsigned char negative_unsigned{-1};
    bool from_pointer{&source};
    bool from_array{source_array};
    short constant_user_conversion{OutOfRangeIntegerConversion{}};
    short runtime_user_conversion{RuntimeIntegerConversion{}};
    int floating_user_conversion{FloatingConversion{}};
    unsigned char enum_negative{list_narrowing_negative};
    float enum_inexact_float{list_narrowing_inexact_float};
    float unsigned_enum_inexact_float{
        list_narrowing_unsigned_inexact_float};
    signed char signed_bitfield_out_of_range{signed_wide.value};
    unsigned char unsigned_bitfield_out_of_range{unsigned_wide.value};
    unsigned char signed_bitfield_to_unsigned{signed_narrow.value};
    bool signed_bitfield_to_bool{signed_narrow.value};
    return from_float.value + nested_out_of_range.nested.byte +
           sizeof(constructor_from_float) + out_of_range +
           array_out_of_range[0] + (int)inexact_integer +
           (int)runtime_narrow + (int)floating_overflow +
           nonzero_integer + from_pointer + from_array + enum_negative +
           signed_overflow + signed_underflow + negative_unsigned +
           (int)enum_inexact_float + (int)unsigned_enum_inexact_float +
           (int)inexact_ull_max + signed_bitfield_out_of_range +
           unsigned_bitfield_out_of_range + signed_bitfield_to_unsigned +
           signed_bitfield_to_bool;
}

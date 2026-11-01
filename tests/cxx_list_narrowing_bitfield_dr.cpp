// CWG 2627's C++23 defect resolution also applies to the earlier language
// modes. GCC 13.2 still diagnoses these conversions as narrowing in both
// -std=c++20 and -std=c++23, so it cannot serve as the positive oracle here.
enum ListNarrowingBitfieldEnum : unsigned int {
    list_narrowing_bitfield_enum_value = 255u
};

struct ListNarrowingBitfieldValues {
    int signed_four : 4;
    unsigned int unsigned_eight : 8;
    unsigned int unsigned_seven : 7;
    unsigned int unsigned_one : 1;
    ListNarrowingBitfieldEnum enum_eight : 8;
};

int main() {
    ListNarrowingBitfieldValues first{};
    ListNarrowingBitfieldValues second{};
    first.signed_four = -7;
    second.signed_four = 6;
    first.unsigned_eight = 255;
    first.unsigned_seven = 127;
    first.unsigned_one = 1;
    first.enum_eight = list_narrowing_bitfield_enum_value;

    bool select_first = first.signed_four < 0;
    int comma_evaluations = 0;
    signed char signed_value{first.signed_four};
    signed char signed_from_unsigned_value{first.unsigned_seven};
    unsigned char unsigned_value{first.unsigned_eight};
    unsigned char enum_value{first.enum_eight};
    bool bool_value{first.unsigned_one};
    signed char conditional_value{
        select_first ? first.signed_four : second.signed_four};
    signed char comma_value{(++comma_evaluations, first.signed_four)};

    if (signed_value != -7 || signed_from_unsigned_value != 127 ||
        unsigned_value != 255 || enum_value != 255 || !bool_value ||
        conditional_value != -7 || comma_value != -7 ||
        comma_evaluations != 1) {
        return 1;
    }
    return 0;
}

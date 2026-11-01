enum EnumFloatValue {
    enum_float_exact = 16777216
};

enum EnumUnsignedFloatValue : unsigned int {
    enum_unsigned_float_exact = 4026531840u
};

float enum_float_take(float value) {
    return value;
}

int enum_float_pick(int) {
    return 1;
}

int enum_float_pick(float) {
    return 2;
}

float enum_float_argument(EnumFloatValue value) {
    return enum_float_take(value);
}

int enum_float_promotion_rank(EnumFloatValue value) {
    return enum_float_pick(value);
}

float enum_unsigned_float_conversion(EnumUnsignedFloatValue value) {
    return value;
}

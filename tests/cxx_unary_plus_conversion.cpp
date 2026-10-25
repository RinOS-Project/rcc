struct UnaryPlusOther {
    int value;
};

int operator+(UnaryPlusOther value) { return value.value + 2; }

struct UnaryPlusIntConversion {
    operator int() const { return 43; }
};

struct UnaryPlusFloatConversion {
    operator float() const { return 1.5f; }
};

int select_unary_plus_type(int) { return 1; }
int select_unary_plus_type(long) { return 2; }
int select_unary_plus_type(float) { return 3; }
int select_unary_plus_type(double) { return 4; }

int unary_plus_conversion_probe() {
    UnaryPlusIntConversion int_value = {};
    UnaryPlusFloatConversion float_value = {};
    return +int_value == 43 && +float_value == 1.5f &&
                   select_unary_plus_type(+int_value) == 1 &&
                   select_unary_plus_type(+float_value) == 3
               ? 0
               : 1;
}

int main() {
    return unary_plus_conversion_probe();
}

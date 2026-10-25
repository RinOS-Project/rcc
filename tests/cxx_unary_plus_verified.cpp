struct VerifiedUnaryPlus {
    int value;
    int operator+() const { return value + 1; }
};

template <typename T>
int verified_unary_plus_template(T value) {
    return +value;
}

template <int Value>
int verified_unary_plus_constant() {
    return +Value;
}

int verified_unary_plus_probe() {
    VerifiedUnaryPlus value = {40};
    return +value == 41 && verified_unary_plus_template(42) == 42 &&
                   verified_unary_plus_constant<43>() == 43
               ? 0
               : 1;
}

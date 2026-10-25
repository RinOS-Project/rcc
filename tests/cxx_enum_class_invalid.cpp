/* Scoped enumerations must not use the implicit C integer conversions. */
enum class Color { red = 3 };
enum class Mode { cold = 1 };

int return_enum() {
    return Color::red;
}

int initialize_enum() {
    int value = Color::red;
    return value;
}

int assign_enum() {
    Color color = Color::red;
    color = 1;
    return 0;
}

int compare_enum() {
    return Color::red == Mode::cold;
}

int branch_enum() {
    if (Color::red) return 1;
    return 0;
}

int subscript_enum() {
    int values[8] = {};
    return values[Color::red];
}

int unary_plus_enum() {
    return +Color::red;
}

struct AmbiguousUnaryPlus {
    operator int() const { return 1; }
    operator float() const { return 1.0f; }
};

int unary_plus_ambiguous_conversion(AmbiguousUnaryPlus value) {
    return +value;
}

struct ExplicitUnaryPlus {
    explicit operator int() const { return 1; }
};

int unary_plus_explicit_conversion(ExplicitUnaryPlus value) {
    return +value;
}

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

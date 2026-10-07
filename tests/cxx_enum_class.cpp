/* C++ scoped-enum declaration, qualification, and type checking coverage. */
enum class Color {
    red = 3,
    blue = 7,
};

enum class Mode : unsigned {
    cold = 1,
    hot = 2,
};

enum ArithmeticEnumerator {
    first = 1 + 2 * 3,
    second = first << 1,
    third = second == 14 ? first : 0,
};

int cxx_enum_class_probe() {
    Color color = Color::blue;
    Mode mode = Mode::hot;
    return color == Color::blue && mode == Mode::hot &&
                   first == 7 && second == 14 && third == 7
        ? 0 : 1;
}

int main() {
    return cxx_enum_class_probe();
}

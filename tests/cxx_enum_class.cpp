/* C++ scoped-enum declaration, qualification, and type checking coverage. */
enum class Color {
    red = 3,
    blue = 7,
};

enum class Mode : unsigned {
    cold = 1,
    hot = 2,
};

int cxx_enum_class_probe() {
    Color color = Color::blue;
    Mode mode = Mode::hot;
    return color == Color::blue && mode == Mode::hot ? 0 : 1;
}

int main() {
    return cxx_enum_class_probe();
}

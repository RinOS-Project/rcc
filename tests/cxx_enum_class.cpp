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

enum class UnsignedByte : unsigned char {
    maximum = 255,
};

enum class SignedByte : signed char {
    minimum = -128,
    maximum = 127,
};

enum UnscopedWideUnsigned : unsigned long long {
    wide_value = 0x100000000ULL,
};

int cxx_enum_class_probe() {
    Color color = Color::blue;
    Mode mode = Mode::hot;
    return color == Color::blue && mode == Mode::hot &&
                   first == 7 && second == 14 && third == 7 &&
                   static_cast<unsigned>(UnsignedByte::maximum) == 255u &&
                   static_cast<int>(SignedByte::minimum) == -128 &&
                   static_cast<int>(SignedByte::maximum) == 127 &&
                   wide_value == 0x100000000ULL
        ? 0 : 1;
}

int main() {
    return cxx_enum_class_probe();
}

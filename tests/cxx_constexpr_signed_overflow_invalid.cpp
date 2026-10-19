constexpr int add_overflow = 2147483647 + 1;
constexpr int subtract_overflow = (-2147483647 - 1) - 1;
constexpr int multiply_overflow = 46341 * 46341;
constexpr int negate_overflow = -(-2147483647 - 1);
constexpr int shift_overflow = 1 << 31;
constexpr int divide_overflow = (-2147483647 - 1) / -1;
constexpr int modulo_overflow = (-2147483647 - 1) % -1;

constexpr int compound_add_overflow() {
    int value = 2147483647;
    value += 1;
    return value;
}

constexpr int compound_overflow = compound_add_overflow();

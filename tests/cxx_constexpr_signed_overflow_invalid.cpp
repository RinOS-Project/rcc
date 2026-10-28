constexpr int add_overflow = 2147483647 + 1;
constexpr int subtract_overflow = (-2147483647 - 1) - 1;
constexpr int multiply_overflow = 46341 * 46341;
constexpr int negate_overflow = -(-2147483647 - 1);
constexpr int shift_overflow = 1 << 31;
constexpr int negative_shift = (-1) << 1;
constexpr unsigned int oversized_shift = 1u << 32;
constexpr int divide_overflow = (-2147483647 - 1) / -1;
constexpr int modulo_overflow = (-2147483647 - 1) % -1;

constexpr int compound_add_overflow() {
    int value = 2147483647;
    value += 1;
    return value;
}

constexpr int compound_overflow = compound_add_overflow();

constexpr unsigned int compound_oversized_shift() {
    unsigned int value = 1u;
    value <<= 32;
    return value;
}

constexpr unsigned int compound_shift_overflow =
    compound_oversized_shift();

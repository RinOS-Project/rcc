// CWG 2864 allows a constant floating conversion when both values are
// non-finite. GCC currently rejects the infinity case in C++20 mode.
constexpr double positive_infinity = __builtin_huge_val();
constexpr double quiet_nan = __builtin_nan("");

int main() {
    float narrowed_infinity{positive_infinity};
    float narrowed_nan{quiet_nan};
    if (!__builtin_isinf(narrowed_infinity)) return 1;
    if (!__builtin_isnan(narrowed_nan)) return 2;
    return 0;
}

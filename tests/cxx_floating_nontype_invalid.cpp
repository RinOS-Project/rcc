template<float Value>
struct FloatBox {};

template<float Value>
int float_function_nttp() {
    return 0;
}

constexpr double double_value = 1.25;
FloatBox<1.0> converts_double_to_float;
FloatBox<1> converts_integer_to_float;
FloatBox<double_value> converts_named_double_to_float;

int converts_explicit_double_to_float() {
    return float_function_nttp<1.0>();
}

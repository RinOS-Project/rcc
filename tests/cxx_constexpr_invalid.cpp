/* A constexpr object must be initialized by the supported constant evaluator. */

constexpr int missing_initializer;
constexpr float division_by_zero = 1.0f / 0.0f;
constexpr int out_of_range = static_cast<int>(2147483648.0);

consteval int require_constant(int value) {
    return value + 1;
}

int invalid_consteval_call(int value) {
    return require_constant(value);
}

constexpr int invalid_constexpr_heap_read() {
    int* value = new int;
    return *value;
}

constexpr int invalid_dynamic_read = invalid_constexpr_heap_read();

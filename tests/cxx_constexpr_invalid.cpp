/* A constexpr object must be initialized by the supported constant evaluator. */

constexpr int missing_initializer;
constexpr float division_by_zero = 1.0f / 0.0f;
constexpr int out_of_range = static_cast<int>(2147483648.0);

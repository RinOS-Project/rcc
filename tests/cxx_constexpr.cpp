/* Scalar constexpr folding coverage. */

constexpr int base_value = 12;

constexpr int select_value(int value) {
    return value > 10 ? value * 2 + 3 : value + 1;
}

constexpr int local_value(int value) {
    int adjusted = value + 1;
    if (adjusted > 10) return adjusted * 2 + 1;
    return adjusted;
}

constexpr int mutate_value(int value) {
    int result = value;
    result += 2;
    ++result;
    return result;
}

constexpr int sum_value(int value) {
    int total = 0;
    for (int index = 0; index < value; ++index) {
        total += index;
    }
    return total;
}

constexpr int while_value(int limit) {
    int index = 0;
    int total = 0;
    while (index < limit) {
        ++index;
        if (index == 2) continue;
        if (index == 5) break;
        total += index;
    }
    return total;
}

constexpr int do_value(int limit) {
    int index = 0;
    do {
        ++index;
    } while (index < limit);
    return index;
}

constexpr int switch_value(int value) {
    int result = 0;
    switch (value) {
        case 1:
            result += 2;
        case 2:
            result += 3;
            break;
        default:
            result = 7;
            break;
    }
    return result;
}

constexpr float scale_float(float value) {
    return value * 2.0f + 0.5f;
}

constexpr double half_integer(int value) {
    return static_cast<double>(value) / 2.0 + 0.25;
}

constexpr int truncate_float(float value) {
    return static_cast<int>(value);
}

consteval int force_constant(int value) {
    return value + 4;
}

consteval int increment_immediate(int value) {
    return value + 1;
}

consteval int compose_immediate(int value) {
    return increment_immediate(value);
}

constexpr float local_float(int value) {
    float result = static_cast<float>(value);
    result += 0.5f;
    if (result > 2.0f) result *= 2.0f;
    return result;
}

constexpr double loop_float(int limit) {
    double total = 0.0;
    for (int index = 0; index < limit; ++index) {
        total += 0.5;
    }
    return total;
}

constexpr bool unsigned_less_than_one(unsigned long long value) {
    return value < 1ULL;
}

constexpr bool unsigned_exceeds_signed_max(unsigned long long value) {
    return value > 9223372036854775807ULL;
}

constexpr unsigned long long unsigned_shift_switch(
    unsigned long long value) {
    switch (value) {
        case (1ULL << 63):
            return value >> 63;
        default:
            return 0ULL;
    }
}

constexpr unsigned long long unsigned_shift_assignment(
    unsigned long long value) {
    value <<= 63;
    value >>= 63;
    return value;
}

constexpr int dynamic_value() {
    int* value = new int;
    *value = 41;
    int result = *value + 1;
    delete value;
    return result;
}

constexpr int dynamic_array_value() {
    int* values = new int[3]{10, 20, 12};
    int result = values[0] + values[1] + values[2];
    delete[] values;
    return result;
}

struct DynamicPair {
    int first;
    int second;
};

template<typename T>
struct DynamicTemplatePair {
    T first;
    T second;
};

constexpr int dynamic_aggregate_value() {
    DynamicPair* pair = new DynamicPair{20, 22};
    int result = pair->first + pair->second;
    delete pair;
    return result;
}

constexpr int dynamic_aggregate_array_value() {
    DynamicPair* pairs = new DynamicPair[2]{
        DynamicPair{1, 2}, DynamicPair{3, 4}};
    int result = pairs[0].second + pairs[1].first;
    delete[] pairs;
    return result;
}

constexpr int dynamic_template_aggregate_value() {
    DynamicTemplatePair<int>* pair =
        new DynamicTemplatePair<int>{20, 22};
    int result = pair->first + pair->second;
    delete pair;
    return result;
}

constexpr int constexpr_global = select_value(base_value);
constexpr int constexpr_local = local_value(base_value);
constexpr int constexpr_mutated = mutate_value(base_value);
constexpr int constexpr_loop = sum_value(5);
constexpr int constexpr_while = while_value(8);
constexpr int constexpr_do = do_value(5);
constexpr int constexpr_switch_one = switch_value(1);
constexpr int constexpr_switch_two = switch_value(2);
constexpr int constexpr_switch_other = switch_value(9);
constexpr float constexpr_float = scale_float(1.5f);
constexpr double constexpr_double = half_integer(7);
constexpr int constexpr_truncated = truncate_float(3.75f);
constexpr int constexpr_consteval = force_constant(3);
constexpr int constexpr_nested_consteval = compose_immediate(41);
constexpr int constexpr_address_consteval =
    (&increment_immediate)(41);
constexpr float constexpr_local_float = local_float(2);
constexpr double constexpr_loop_float = loop_float(4);
constexpr int constexpr_dynamic = dynamic_value();
constexpr int constexpr_dynamic_array = dynamic_array_value();
constexpr int constexpr_dynamic_aggregate = dynamic_aggregate_value();
constexpr int constexpr_dynamic_aggregate_array =
    dynamic_aggregate_array_value();
constexpr int constexpr_dynamic_template_aggregate =
    dynamic_template_aggregate_value();
constexpr bool constexpr_conditional_unsigned_comparison =
    (true ? -1 : 0u) < 0;
constexpr bool constexpr_ull_max_less_than_one =
    unsigned_less_than_one(18446744073709551615ULL);
constexpr bool constexpr_ull_max_exceeds_signed_max =
    unsigned_exceeds_signed_max(18446744073709551615ULL);
constexpr unsigned long long constexpr_ull_wrapped_sum =
    18446744073709551615ULL + 1ULL;
constexpr bool constexpr_ull_wrapped_sum_is_zero =
    constexpr_ull_wrapped_sum == 0ULL;
constexpr bool constexpr_ull_shift_keeps_top_bit =
    (18446744073709551615ULL >> 63) == 1ULL;
constexpr bool constexpr_ull_shift_switch =
    unsigned_shift_switch(1ULL << 63) == 1ULL;
constexpr bool constexpr_ull_shift_assignment =
    unsigned_shift_assignment(1ULL) == 1ULL;
constexpr bool constexpr_ull_conditional_preserves_unsigned =
    (true ? 18446744073709551615ULL : 0ULL) >
    9223372036854775807ULL;

constexpr int constexpr_int_max = 2147483647;
constexpr int constexpr_int_min = -2147483647 - 1;

constexpr signed char constexpr_increment_signed_char_max() {
    signed char value = 127;
    ++value;
    return value;
}

constexpr signed char constexpr_signed_char_incremented =
    constexpr_increment_signed_char_max();

static_assert(constexpr_dynamic_aggregate == 42);
static_assert(constexpr_dynamic_aggregate_array == 5);
static_assert(constexpr_dynamic_template_aggregate == 42);
static_assert(constexpr_nested_consteval == 42);
static_assert(constexpr_address_consteval == 42);
static_assert(!constexpr_conditional_unsigned_comparison);
static_assert(!constexpr_ull_max_less_than_one);
static_assert(constexpr_ull_max_exceeds_signed_max);
static_assert(constexpr_ull_wrapped_sum_is_zero);
static_assert(constexpr_ull_shift_keeps_top_bit);
static_assert(constexpr_ull_shift_switch);
static_assert(constexpr_ull_shift_assignment);
static_assert(constexpr_ull_conditional_preserves_unsigned);
static_assert(constexpr_int_max == 2147483647);
static_assert(constexpr_int_min == (-2147483647 - 1));
static_assert(constexpr_signed_char_incremented == -128);

int main(void) {
    return constexpr_global == 27 && constexpr_local == 27 &&
                   constexpr_mutated == 15 && constexpr_loop == 10 &&
                   constexpr_while == 8 && constexpr_do == 5 &&
                   constexpr_switch_one == 5 && constexpr_switch_two == 3 &&
                   constexpr_switch_other == 7 &&
                   constexpr_float == 3.5f && constexpr_double == 3.75 &&
                   constexpr_truncated == 3 && constexpr_consteval == 7 &&
                   constexpr_nested_consteval == 42 &&
                   constexpr_address_consteval == 42 &&
                   constexpr_local_float == 5.0f &&
                   constexpr_loop_float == 2.0 &&
                   constexpr_dynamic == 42 &&
                   constexpr_dynamic_array == 42 &&
                   constexpr_dynamic_aggregate == 42 &&
                   constexpr_dynamic_aggregate_array == 5 &&
                   constexpr_dynamic_template_aggregate == 42 &&
                   constexpr_ull_wrapped_sum_is_zero &&
                   constexpr_ull_shift_keeps_top_bit &&
                   constexpr_ull_shift_switch &&
                   constexpr_ull_shift_assignment &&
                   constexpr_ull_conditional_preserves_unsigned &&
                   constexpr_int_max == 2147483647 &&
                   constexpr_int_min == (-2147483647 - 1) &&
                   constexpr_signed_char_incremented == -128 &&
                   !constexpr_conditional_unsigned_comparison &&
                   !constexpr_ull_max_less_than_one &&
                   constexpr_ull_max_exceeds_signed_max &&
                   dynamic_aggregate_value() == 42 &&
                   dynamic_aggregate_array_value() == 5
               ? 0 : 1;
}

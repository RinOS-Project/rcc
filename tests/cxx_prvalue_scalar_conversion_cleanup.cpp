static int argument_evaluations;
static int pointer_argument_evaluations;
static int wide_argument_evaluations;
static int float_argument_evaluations;
static int double_argument_evaluations;
static int constructions;
static int conversions;
static int destructions;
static int constructor_argument_mismatches;
static int pointer_target;
static int argument_source_constructions;
static int argument_source_conversions;
static int argument_source_destructions;
static int premature_argument_source_destructions;
static int wide_aggregate_first_evaluations;
static int wide_aggregate_second_evaluations;
static int wide_aggregate_third_evaluations;

static int next_prvalue_value() {
    ++argument_evaluations;
    return 41;
}

static int* next_prvalue_pointer() {
    ++pointer_argument_evaluations;
    return &pointer_target;
}

static long long next_prvalue_wide_value() {
    ++wide_argument_evaluations;
    return 0x123456789abcdefLL;
}

static float next_prvalue_float_value() {
    ++float_argument_evaluations;
    return 13.5f;
}

static double next_prvalue_double_value() {
    ++double_argument_evaluations;
    return 27.25;
}

static long long next_wide_aggregate_first() {
    ++wide_aggregate_first_evaluations;
    return 101;
}

static long long next_wide_aggregate_second() {
    ++wide_aggregate_second_evaluations;
    return 202;
}

static long long next_wide_aggregate_third() {
    ++wide_aggregate_third_evaluations;
    return 303;
}

struct PrvalueScalarConversion {
    int value;
    int* marker;
    long long wide_value;
    float float_value;
    double double_value;

    explicit PrvalueScalarConversion(int initial, int* initial_marker,
                                     long long initial_wide,
                                     float initial_float,
                                     double initial_double) noexcept
        : value(initial), marker(initial_marker), wide_value(initial_wide),
          float_value(initial_float), double_value(initial_double) {
        ++constructions;
        if (initial != value || initial_marker != marker ||
            initial_wide != wide_value || initial_float != float_value ||
            initial_double != double_value) {
            ++constructor_argument_mismatches;
        }
    }

    operator int() && noexcept {
        ++conversions;
        return value;
    }

    ~PrvalueScalarConversion() noexcept {
        ++destructions;
    }
};

struct ConstructorArgumentSource {
    int value;

    explicit ConstructorArgumentSource(int initial) : value(initial) {
        ++argument_source_constructions;
    }

    operator int() && {
        ++argument_source_conversions;
        return value;
    }

    ~ConstructorArgumentSource() {
        ++argument_source_destructions;
    }
};

struct ConstructorArgumentReuse {
    int from_initializer;
    int from_body;

    explicit ConstructorArgumentReuse(int value)
        : from_initializer(value) {
        if (argument_source_destructions !=
            argument_source_constructions - 1) {
            ++premature_argument_source_destructions;
        }
        from_body = value;
    }
};

struct WideConstructorArgument {
    long long first;
    long long second;
    long long third;
};

static long long sum_wide_constructor_argument(
    WideConstructorArgument value) {
    return value.first + value.second + value.third;
}

struct WideConstructorArgumentReuse {
    long long from_initializer_first;
    long long from_initializer_second;
    long long from_body_third;
    long long from_body_sum;

    explicit WideConstructorArgumentReuse(WideConstructorArgument value)
        : from_initializer_first(value.first),
          from_initializer_second(value.second) {
        from_body_third = value.third;
        from_body_sum = value.first + value.second + value.third;
    }
};

int main() {
    int direct = PrvalueScalarConversion{
        next_prvalue_value(), next_prvalue_pointer(),
        next_prvalue_wide_value(), next_prvalue_float_value(),
        next_prvalue_double_value()};
    if (direct != 41) return 11;
    if (argument_evaluations != 1) return 12;
    if (pointer_argument_evaluations != 1) return 17;
    if (wide_argument_evaluations != 1) return 18;
    if (float_argument_evaluations != 1) return 19;
    if (double_argument_evaluations != 1) return 20;
    if (constructions != 1) return 13;
    if (conversions != 1) return 14;
    if (destructions != 1) return 15;
    if (constructor_argument_mismatches != 0) return 16;

    int listed = {PrvalueScalarConversion{
        next_prvalue_value(), next_prvalue_pointer(),
        next_prvalue_wide_value(), next_prvalue_float_value(),
        next_prvalue_double_value()}};
    if (listed != 41) return 21;
    if (argument_evaluations != 2) return 22;
    if (pointer_argument_evaluations != 2) return 27;
    if (wide_argument_evaluations != 2) return 28;
    if (float_argument_evaluations != 2) return 29;
    if (double_argument_evaluations != 2) return 30;
    if (constructions != 2) return 23;
    if (conversions != 2) return 24;
    if (destructions != 2) return 25;
    if (constructor_argument_mismatches != 0) return 26;

    ConstructorArgumentReuse reused(
        static_cast<int>(ConstructorArgumentSource{67}));
    if (reused.from_initializer != 67 || reused.from_body != 67) return 31;
    if (argument_source_constructions != 1) return 32;
    if (argument_source_conversions != 1) return 33;
    if (argument_source_destructions != 1) return 34;
    if (premature_argument_source_destructions != 0) return 35;

    ConstructorArgumentReuse implicit_reused(ConstructorArgumentSource{73});
    if (implicit_reused.from_initializer != 73 ||
        implicit_reused.from_body != 73) return 36;
    if (argument_source_constructions != 2) return 37;
    if (argument_source_conversions != 2) return 38;
    if (argument_source_destructions != 2) return 39;
    if (premature_argument_source_destructions != 0) return 40;

    WideConstructorArgumentReuse wide_reused(WideConstructorArgument{
        next_wide_aggregate_first(), next_wide_aggregate_second(),
        next_wide_aggregate_third()});
    if (wide_reused.from_initializer_first != 101 ||
        wide_reused.from_initializer_second != 202 ||
        wide_reused.from_body_third != 303 ||
        wide_reused.from_body_sum != 606) return 41;
    if (wide_aggregate_first_evaluations != 1 ||
        wide_aggregate_second_evaluations != 1 ||
        wide_aggregate_third_evaluations != 1) return 42;
    if (sum_wide_constructor_argument(WideConstructorArgument{
            next_wide_aggregate_first(), next_wide_aggregate_second(),
            next_wide_aggregate_third()}) != 606) return 43;
    if (wide_aggregate_first_evaluations != 2 ||
        wide_aggregate_second_evaluations != 2 ||
        wide_aggregate_third_evaluations != 2) return 44;

    return 0;
}

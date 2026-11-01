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
static int wide_comma_effect_evaluations;
static int wide_selected_factory_evaluations;
static int wide_alternate_factory_evaluations;

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

static int nontrivial_wide_factory_evaluations;
static int nontrivial_wide_destructor_evaluations;

struct NontrivialWideConstructorArgument {
    long long first;
    long long second;
    long long third;

    ~NontrivialWideConstructorArgument() {
        ++nontrivial_wide_destructor_evaluations;
        first = -1;
    }
};

static NontrivialWideConstructorArgument
make_nontrivial_wide_constructor_argument() {
    ++nontrivial_wide_factory_evaluations;
    return NontrivialWideConstructorArgument{701, 702, 703};
}

static long long sum_wide_constructor_argument(
    WideConstructorArgument value) {
    return value.first + value.second + value.third;
}

static WideConstructorArgument make_wide_constructor_argument() {
    ++wide_selected_factory_evaluations;
    return WideConstructorArgument{11, 22, 33};
}

static WideConstructorArgument
make_alternate_wide_constructor_argument() {
    ++wide_alternate_factory_evaluations;
    return WideConstructorArgument{111, 222, 333};
}

static int record_wide_comma_effect() {
    ++wide_comma_effect_evaluations;
    return 0;
}

struct NestedWideConstructorArgument {
    WideConstructorArgument inner;
    long long fourth;
};

static long long sum_nested_wide_constructor_argument(
    NestedWideConstructorArgument value) {
    return value.inner.first + value.inner.second + value.inner.third +
        value.fourth;
}

static NestedWideConstructorArgument
make_nested_wide_constructor_argument() {
    return NestedWideConstructorArgument{
        WideConstructorArgument{11, 22, 33}, 44};
}

struct WideArrayConstructorArgument {
    long long values[4];
};

static long long sum_wide_array_constructor_argument(
    WideArrayConstructorArgument value) {
    return value.values[0] + value.values[1] + value.values[2] +
        value.values[3];
}

static WideArrayConstructorArgument
make_wide_array_constructor_argument() {
    return WideArrayConstructorArgument{{11, 22, 33, 44}};
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

struct NontrivialWideConstructorArgumentReuse {
    long long from_initializer_first;
    long long from_initializer_second;
    long long from_body_third;
    long long from_body_sum;

    explicit NontrivialWideConstructorArgumentReuse(
        NontrivialWideConstructorArgument value)
        : from_initializer_first(value.first),
          from_initializer_second(value.second) {
        from_body_third = value.third;
        from_body_sum = value.first + value.second + value.third;
    }
};

static int explicit_copy_wide_constructor_evaluations;
static int explicit_copy_wide_destructor_evaluations;

struct ExplicitCopyWideConstructorArgument {
    long long first;
    long long second;
    long long third;

    ExplicitCopyWideConstructorArgument(
        long long initial_first, long long initial_second,
        long long initial_third)
        : first(initial_first), second(initial_second), third(initial_third) {}

    ExplicitCopyWideConstructorArgument(
        const ExplicitCopyWideConstructorArgument& other)
        : first(other.first + 50), second(other.second), third(other.third) {
        ++explicit_copy_wide_constructor_evaluations;
    }

    ~ExplicitCopyWideConstructorArgument() {
        ++explicit_copy_wide_destructor_evaluations;
        first = -1;
    }
};

struct ExplicitCopyWideMemberSource {
    ExplicitCopyWideConstructorArgument first;
    ExplicitCopyWideConstructorArgument second;
};

static ExplicitCopyWideConstructorArgument explicit_copy_wide_prvalue_factory()
    noexcept {
    return ExplicitCopyWideConstructorArgument(2501, 2502, 2503);
}

struct ExplicitCopyWideConstructorArgumentReuse {
    long long from_initializer_first;
    long long from_initializer_second;
    long long from_body_third;
    long long from_body_sum;

    explicit ExplicitCopyWideConstructorArgumentReuse(
        ExplicitCopyWideConstructorArgument value)
        : from_initializer_first(value.first),
          from_initializer_second(value.second) {
        from_body_third = value.third;
        from_body_sum = value.first + value.second + value.third;
    }
};

static int explicit_copy_wide_parameter_sum(
    ExplicitCopyWideConstructorArgument value) {
    return static_cast<int>(value.first) +
        static_cast<int>(value.second) + static_cast<int>(value.third);
}

static int explicit_move_wide_constructor_evaluations;
static int explicit_move_wide_destructor_evaluations;

struct ExplicitMoveWideConstructorArgument {
    long long first;
    long long second;
    long long third;

    ExplicitMoveWideConstructorArgument(
        long long initial_first, long long initial_second,
        long long initial_third)
        : first(initial_first), second(initial_second), third(initial_third) {}

    ExplicitMoveWideConstructorArgument(
        ExplicitMoveWideConstructorArgument&& other)
        : first(other.first + 70), second(other.second), third(other.third) {
        other.first = -2;
        ++explicit_move_wide_constructor_evaluations;
    }

    ~ExplicitMoveWideConstructorArgument() {
        ++explicit_move_wide_destructor_evaluations;
        first = -1;
    }
};

struct ExplicitMoveWideMemberSource {
    ExplicitMoveWideConstructorArgument first;
    ExplicitMoveWideConstructorArgument second;
};

static ExplicitMoveWideConstructorArgument explicit_move_wide_prvalue_factory()
    noexcept {
    return ExplicitMoveWideConstructorArgument(2601, 2602, 2603);
}

static int throwing_copy_parameter_constructor_evaluations;
static int throwing_copy_parameter_destructor_evaluations;
static int throwing_copy_parameter_should_throw;
static int throwing_copy_parameter_callee_evaluations;

struct ThrowingCopyFunctionParameter {
    long long first;
    long long second;
    long long third;

    ThrowingCopyFunctionParameter(
        long long initial_first, long long initial_second,
        long long initial_third)
        : first(initial_first), second(initial_second), third(initial_third) {}

    ThrowingCopyFunctionParameter(
        const ThrowingCopyFunctionParameter& other)
        : first(other.first), second(other.second), third(other.third) {
        ++throwing_copy_parameter_constructor_evaluations;
        if (throwing_copy_parameter_should_throw &&
            throwing_copy_parameter_constructor_evaluations == 2) throw 93;
    }

    ~ThrowingCopyFunctionParameter() {
        ++throwing_copy_parameter_destructor_evaluations;
    }
};

static int consume_throwing_copy_parameters(
    ThrowingCopyFunctionParameter first,
    ThrowingCopyFunctionParameter second) {
    ++throwing_copy_parameter_callee_evaluations;
    return static_cast<int>(first.first) + static_cast<int>(first.second) +
        static_cast<int>(first.third) + static_cast<int>(second.first) +
        static_cast<int>(second.second) + static_cast<int>(second.third);
}

struct ExplicitMoveWideConstructorArgumentReuse {
    long long from_initializer_first;
    long long from_initializer_second;
    long long from_body_third;
    long long from_body_sum;

    explicit ExplicitMoveWideConstructorArgumentReuse(
        ExplicitMoveWideConstructorArgument value)
        : from_initializer_first(value.first),
          from_initializer_second(value.second) {
        from_body_third = value.third;
        from_body_sum = value.first + value.second + value.third;
    }
};

static int explicit_move_wide_parameter_sum(
    ExplicitMoveWideConstructorArgument value) {
    return static_cast<int>(value.first) +
        static_cast<int>(value.second) + static_cast<int>(value.third);
}

struct NestedWideConstructorArgumentReuse {
    long long from_initializer_first;
    long long from_initializer_second;
    long long from_body_third;
    long long from_body_fourth;
    long long from_body_sum;

    explicit NestedWideConstructorArgumentReuse(
        NestedWideConstructorArgument value)
        : from_initializer_first(value.inner.first),
          from_initializer_second(value.inner.second) {
        from_body_third = value.inner.third;
        from_body_fourth = value.fourth;
        from_body_sum = sum_nested_wide_constructor_argument(value);
    }
};

struct WideArrayConstructorArgumentReuse {
    long long from_initializer_first;
    long long from_initializer_second;
    long long from_body_third;
    long long from_body_fourth;
    long long from_body_sum;

    explicit WideArrayConstructorArgumentReuse(
        WideArrayConstructorArgument value)
        : from_initializer_first(value.values[0]),
          from_initializer_second(value.values[1]) {
        from_body_third = value.values[2];
        from_body_fourth = value.values[3];
        from_body_sum = sum_wide_array_constructor_argument(value);
    }
};

struct VolatileWideConstructorArgument {
    volatile long long first;
    volatile long long second;
    volatile long long third;
};

static VolatileWideConstructorArgument volatile_wide_argument{
    1001, 1002, 1003};

static void overwrite_volatile_wide_value(
    volatile long long* destination, long long value) {
    *destination = value;
}

struct VolatileWideConstructorArgumentReuse {
    long long from_initializer;
    long long from_body;
    long long second_from_body;

    explicit VolatileWideConstructorArgumentReuse(
        VolatileWideConstructorArgument value)
        : from_initializer(value.first) {
        overwrite_volatile_wide_value(&value.first, 1004);
        from_body = value.first;
        second_from_body = value.second;
    }
};

struct ConstWideConstructorArgumentReuse {
    long long from_initializer;
    long long from_body;

    explicit ConstWideConstructorArgumentReuse(
        const WideConstructorArgument value)
        : from_initializer(value.first) {
        from_body = value.second;
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
    if (sum_wide_constructor_argument(
            make_wide_constructor_argument()) != 66) return 45;

    NestedWideConstructorArgumentReuse nested_wide_reused(
        NestedWideConstructorArgument{
            WideConstructorArgument{
                next_wide_aggregate_first(), next_wide_aggregate_second(),
                next_wide_aggregate_third()},
            404});
    if (nested_wide_reused.from_initializer_first != 101 ||
        nested_wide_reused.from_initializer_second != 202 ||
        nested_wide_reused.from_body_third != 303 ||
        nested_wide_reused.from_body_fourth != 404 ||
        nested_wide_reused.from_body_sum != 1010) return 46;
    if (wide_aggregate_first_evaluations != 3 ||
        wide_aggregate_second_evaluations != 3 ||
        wide_aggregate_third_evaluations != 3) return 47;
    if (sum_nested_wide_constructor_argument(
            make_nested_wide_constructor_argument()) != 110) return 48;

    WideArrayConstructorArgumentReuse wide_array_reused(
        WideArrayConstructorArgument{{
            next_wide_aggregate_first(), next_wide_aggregate_second(),
            next_wide_aggregate_third(), 404}});
    if (wide_array_reused.from_initializer_first != 101 ||
        wide_array_reused.from_initializer_second != 202 ||
        wide_array_reused.from_body_third != 303 ||
        wide_array_reused.from_body_fourth != 404 ||
        wide_array_reused.from_body_sum != 1010) return 49;
    if (wide_aggregate_first_evaluations != 4 ||
        wide_aggregate_second_evaluations != 4 ||
        wide_aggregate_third_evaluations != 4) return 50;
    if (sum_wide_array_constructor_argument(
            make_wide_array_constructor_argument()) != 110) return 51;
    if (sum_wide_array_constructor_argument(
            {{11, 22, 33, 44}}) != 110) return 52;

    VolatileWideConstructorArgumentReuse volatile_reused(
        volatile_wide_argument);
    if (volatile_reused.from_initializer != 1001 ||
        volatile_reused.from_body != 1004 ||
        volatile_reused.second_from_body != 1002 ||
        volatile_wide_argument.first != 1001) return 53;

    ConstWideConstructorArgumentReuse const_reused(
        WideConstructorArgument{501, 502, 503});
    if (const_reused.from_initializer != 501 ||
        const_reused.from_body != 502) return 54;

    wide_selected_factory_evaluations = 0;
    wide_alternate_factory_evaluations = 0;
    WideConstructorArgumentReuse conditional_wide_reused(
        next_prvalue_value() == 41
            ? make_wide_constructor_argument()
            : make_alternate_wide_constructor_argument());
    if (conditional_wide_reused.from_initializer_first != 11 ||
        conditional_wide_reused.from_initializer_second != 22 ||
        conditional_wide_reused.from_body_third != 33 ||
        conditional_wide_reused.from_body_sum != 66 ||
        wide_selected_factory_evaluations != 1 ||
        wide_alternate_factory_evaluations != 0) return 55;

    WideConstructorArgumentReuse alternate_conditional_wide_reused(
        next_prvalue_value() == 42
            ? make_wide_constructor_argument()
            : make_alternate_wide_constructor_argument());
    if (alternate_conditional_wide_reused.from_initializer_first != 111 ||
        alternate_conditional_wide_reused.from_initializer_second != 222 ||
        alternate_conditional_wide_reused.from_body_third != 333 ||
        alternate_conditional_wide_reused.from_body_sum != 666 ||
        wide_selected_factory_evaluations != 1 ||
        wide_alternate_factory_evaluations != 1 ||
        argument_evaluations != 4) return 56;

    wide_comma_effect_evaluations = 0;
    wide_selected_factory_evaluations = 0;
    WideConstructorArgumentReuse comma_wide_reused(
        (record_wide_comma_effect(), make_wide_constructor_argument()));
    if (comma_wide_reused.from_initializer_first != 11 ||
        comma_wide_reused.from_initializer_second != 22 ||
        comma_wide_reused.from_body_third != 33 ||
        comma_wide_reused.from_body_sum != 66 ||
        wide_comma_effect_evaluations != 1 ||
        wide_selected_factory_evaluations != 1) return 57;

    nontrivial_wide_factory_evaluations = 0;
    nontrivial_wide_destructor_evaluations = 0;
    NontrivialWideConstructorArgumentReuse nontrivial_wide_reused(
        make_nontrivial_wide_constructor_argument());
    if (nontrivial_wide_reused.from_initializer_first != 701 ||
        nontrivial_wide_reused.from_initializer_second != 702 ||
        nontrivial_wide_reused.from_body_third != 703 ||
        nontrivial_wide_reused.from_body_sum != 2106) return 58;
    if (nontrivial_wide_factory_evaluations != 1) return 59;
    if (nontrivial_wide_destructor_evaluations != 1) return 60;

    nontrivial_wide_destructor_evaluations = 0;
    {
        NontrivialWideConstructorArgument lvalue_argument{801, 802, 803};
        NontrivialWideConstructorArgumentReuse copied_from_lvalue(
            lvalue_argument);
        if (copied_from_lvalue.from_initializer_first != 801 ||
            copied_from_lvalue.from_initializer_second != 802 ||
            copied_from_lvalue.from_body_third != 803 ||
            copied_from_lvalue.from_body_sum != 2406 ||
            lvalue_argument.first != 801 ||
            nontrivial_wide_destructor_evaluations != 1) return 61;
    }
    if (nontrivial_wide_destructor_evaluations != 2) return 62;

    explicit_copy_wide_constructor_evaluations = 0;
    explicit_copy_wide_destructor_evaluations = 0;
    {
        ExplicitCopyWideConstructorArgument explicit_copy_source(
            901, 902, 903);
        ExplicitCopyWideConstructorArgumentReuse copied_explicitly(
            explicit_copy_source);
        if (copied_explicitly.from_initializer_first != 951 ||
            copied_explicitly.from_initializer_second != 902 ||
            copied_explicitly.from_body_third != 903 ||
            copied_explicitly.from_body_sum != 2756 ||
            explicit_copy_source.first != 901 ||
            explicit_copy_wide_constructor_evaluations != 1 ||
            explicit_copy_wide_destructor_evaluations != 1) return 63;
    }
    if (explicit_copy_wide_destructor_evaluations != 2) return 64;

    explicit_copy_wide_constructor_evaluations = 0;
    explicit_copy_wide_destructor_evaluations = 0;
    {
        ExplicitCopyWideConstructorArgument explicit_copy_call_source(
            701, 702, 703);
        int copied_sum = explicit_copy_wide_parameter_sum(
            explicit_copy_call_source);
        if (copied_sum != 2156 ||
            explicit_copy_call_source.first != 701 ||
            explicit_copy_wide_constructor_evaluations != 1 ||
            explicit_copy_wide_destructor_evaluations != 1) return 67;
    }
    if (explicit_copy_wide_destructor_evaluations != 2) return 68;

    explicit_copy_wide_constructor_evaluations = 0;
    explicit_copy_wide_destructor_evaluations = 0;
    {
        ExplicitCopyWideConstructorArgument conditional_copy_left(
            1301, 1302, 1303);
        ExplicitCopyWideConstructorArgument conditional_copy_right(
            1401, 1402, 1403);
        bool take_left = true;
        int left_sum = explicit_copy_wide_parameter_sum(
            take_left ? conditional_copy_left : conditional_copy_right);
        take_left = false;
        int right_sum = explicit_copy_wide_parameter_sum(
            take_left ? conditional_copy_left : conditional_copy_right);
        if (left_sum != 3956 || right_sum != 4256 ||
            conditional_copy_left.first != 1301 ||
            conditional_copy_right.first != 1401 ||
            explicit_copy_wide_constructor_evaluations != 2 ||
            explicit_copy_wide_destructor_evaluations != 2) return 75;
    }
    if (explicit_copy_wide_destructor_evaluations != 4) return 76;

    explicit_copy_wide_constructor_evaluations = 0;
    explicit_copy_wide_destructor_evaluations = 0;
    {
        ExplicitCopyWideConstructorArgument copy_array[2] = {
            ExplicitCopyWideConstructorArgument(1701, 1702, 1703),
            ExplicitCopyWideConstructorArgument(1801, 1802, 1803)};
        int array_sum = explicit_copy_wide_parameter_sum(copy_array[1]);
        if (array_sum != 5456 || copy_array[0].first != 1701 ||
            copy_array[1].first != 1801 ||
            explicit_copy_wide_constructor_evaluations != 1 ||
            explicit_copy_wide_destructor_evaluations != 1) return 79;
    }
    if (explicit_copy_wide_destructor_evaluations != 3) return 80;

    explicit_copy_wide_constructor_evaluations = 0;
    explicit_copy_wide_destructor_evaluations = 0;
    {
        ExplicitCopyWideMemberSource member_source = {
            {2101, 2102, 2103}, {2201, 2202, 2203}};
        int member_sum = explicit_copy_wide_parameter_sum(member_source.second);
        if (member_sum != 6656 || member_source.first.first != 2101 ||
            member_source.second.first != 2201 ||
            explicit_copy_wide_constructor_evaluations != 1 ||
            explicit_copy_wide_destructor_evaluations != 1) return 83;
    }
    if (explicit_copy_wide_destructor_evaluations != 3) return 84;

    explicit_copy_wide_constructor_evaluations = 0;
    explicit_copy_wide_destructor_evaluations = 0;
    {
        int factory_sum = explicit_copy_wide_parameter_sum(
            explicit_copy_wide_prvalue_factory());
        if (factory_sum != 7506 ||
            explicit_copy_wide_constructor_evaluations != 0 ||
            explicit_copy_wide_destructor_evaluations != 1) return 87;
    }
    if (explicit_copy_wide_destructor_evaluations != 1) return 88;

    explicit_copy_wide_constructor_evaluations = 0;
    explicit_copy_wide_destructor_evaluations = 0;
    {
        int copied_prvalue_sum = explicit_copy_wide_parameter_sum(
            ExplicitCopyWideConstructorArgument(1001, 1002, 1003));
        if (copied_prvalue_sum != 3006 ||
            explicit_copy_wide_constructor_evaluations != 0 ||
            explicit_copy_wide_destructor_evaluations != 1) return 71;
    }
    if (explicit_copy_wide_destructor_evaluations != 1) return 72;

    explicit_move_wide_constructor_evaluations = 0;
    explicit_move_wide_destructor_evaluations = 0;
    {
        ExplicitMoveWideConstructorArgument move_source(
            1001, 1002, 1003);
        ExplicitMoveWideConstructorArgumentReuse moved_explicitly(
            static_cast<ExplicitMoveWideConstructorArgument&&>(
                move_source));
        if (moved_explicitly.from_initializer_first != 1071 ||
            moved_explicitly.from_initializer_second != 1002 ||
            moved_explicitly.from_body_third != 1003 ||
            moved_explicitly.from_body_sum != 3076 ||
            move_source.first != -2 ||
            explicit_move_wide_constructor_evaluations != 1 ||
            explicit_move_wide_destructor_evaluations != 1) return 65;
    }
    if (explicit_move_wide_destructor_evaluations != 2) return 66;

    explicit_move_wide_constructor_evaluations = 0;
    explicit_move_wide_destructor_evaluations = 0;
    {
        ExplicitMoveWideConstructorArgument explicit_move_call_source(
            801, 802, 803);
        int moved_sum = explicit_move_wide_parameter_sum(
            static_cast<ExplicitMoveWideConstructorArgument&&>(
                explicit_move_call_source));
        if (moved_sum != 2476 ||
            explicit_move_call_source.first != -2 ||
            explicit_move_wide_constructor_evaluations != 1 ||
            explicit_move_wide_destructor_evaluations != 1) return 69;
    }
    if (explicit_move_wide_destructor_evaluations != 2) return 70;

    explicit_move_wide_constructor_evaluations = 0;
    explicit_move_wide_destructor_evaluations = 0;
    {
        ExplicitMoveWideConstructorArgument conditional_move_left(
            1501, 1502, 1503);
        ExplicitMoveWideConstructorArgument conditional_move_right(
            1601, 1602, 1603);
        bool take_left = true;
        int left_sum = explicit_move_wide_parameter_sum(
            take_left
                ? static_cast<ExplicitMoveWideConstructorArgument&&>(
                      conditional_move_left)
                : static_cast<ExplicitMoveWideConstructorArgument&&>(
                      conditional_move_right));
        take_left = false;
        int right_sum = explicit_move_wide_parameter_sum(
            take_left
                ? static_cast<ExplicitMoveWideConstructorArgument&&>(
                      conditional_move_left)
                : static_cast<ExplicitMoveWideConstructorArgument&&>(
                      conditional_move_right));
        if (left_sum != 4576 || right_sum != 4876 ||
            conditional_move_left.first != -2 ||
            conditional_move_right.first != -2 ||
            explicit_move_wide_constructor_evaluations != 2 ||
            explicit_move_wide_destructor_evaluations != 2) return 77;
    }
    if (explicit_move_wide_destructor_evaluations != 4) return 78;

    explicit_move_wide_constructor_evaluations = 0;
    explicit_move_wide_destructor_evaluations = 0;
    {
        ExplicitMoveWideConstructorArgument move_array[2] = {
            ExplicitMoveWideConstructorArgument(1901, 1902, 1903),
            ExplicitMoveWideConstructorArgument(2001, 2002, 2003)};
        int array_sum = explicit_move_wide_parameter_sum(
            static_cast<ExplicitMoveWideConstructorArgument&&>(
                move_array[1]));
        if (array_sum != 6076 || move_array[0].first != 1901 ||
            move_array[1].first != -2 ||
            explicit_move_wide_constructor_evaluations != 1 ||
            explicit_move_wide_destructor_evaluations != 1) return 81;
    }
    if (explicit_move_wide_destructor_evaluations != 3) return 82;

    explicit_move_wide_constructor_evaluations = 0;
    explicit_move_wide_destructor_evaluations = 0;
    {
        ExplicitMoveWideMemberSource member_source = {
            {2301, 2302, 2303}, {2401, 2402, 2403}};
        int member_sum = explicit_move_wide_parameter_sum(
            static_cast<ExplicitMoveWideConstructorArgument&&>(
                member_source.second));
        if (member_sum != 7276 || member_source.first.first != 2301 ||
            member_source.second.first != -2 ||
            explicit_move_wide_constructor_evaluations != 1 ||
            explicit_move_wide_destructor_evaluations != 1) return 85;
    }
    if (explicit_move_wide_destructor_evaluations != 3) return 86;

    explicit_move_wide_constructor_evaluations = 0;
    explicit_move_wide_destructor_evaluations = 0;
    {
        int factory_sum = explicit_move_wide_parameter_sum(
            explicit_move_wide_prvalue_factory());
        if (factory_sum != 7806 ||
            explicit_move_wide_constructor_evaluations != 0 ||
            explicit_move_wide_destructor_evaluations != 1) return 89;
    }
    if (explicit_move_wide_destructor_evaluations != 1) return 90;

    explicit_move_wide_constructor_evaluations = 0;
    explicit_move_wide_destructor_evaluations = 0;
    {
        int moved_prvalue_sum = explicit_move_wide_parameter_sum(
            ExplicitMoveWideConstructorArgument(1101, 1102, 1103));
        if (moved_prvalue_sum != 3306 ||
            explicit_move_wide_constructor_evaluations != 0 ||
            explicit_move_wide_destructor_evaluations != 1) return 73;
    }
    if (explicit_move_wide_destructor_evaluations != 1) return 74;

    throwing_copy_parameter_constructor_evaluations = 0;
    throwing_copy_parameter_destructor_evaluations = 0;
    throwing_copy_parameter_callee_evaluations = 0;
    throwing_copy_parameter_should_throw = 1;
    {
        ThrowingCopyFunctionParameter first_source(2701, 2702, 2703);
        ThrowingCopyFunctionParameter second_source(2801, 2802, 2803);
        bool caught = false;
        try {
            (void)consume_throwing_copy_parameters(first_source, second_source);
        } catch (int exception_value) {
            caught = exception_value == 93;
        }
        if (!caught || first_source.first != 2701 ||
            second_source.first != 2801 ||
            throwing_copy_parameter_constructor_evaluations != 2 ||
            throwing_copy_parameter_destructor_evaluations != 1 ||
            throwing_copy_parameter_callee_evaluations != 0) return 91;
    }
    if (throwing_copy_parameter_destructor_evaluations != 3) return 92;

    throwing_copy_parameter_constructor_evaluations = 0;
    throwing_copy_parameter_destructor_evaluations = 0;
    throwing_copy_parameter_callee_evaluations = 0;
    throwing_copy_parameter_should_throw = 0;
    {
        ThrowingCopyFunctionParameter first_source(2901, 2902, 2903);
        ThrowingCopyFunctionParameter second_source(3001, 3002, 3003);
        int sum = consume_throwing_copy_parameters(
            first_source, second_source);
        if (sum != 17712 || first_source.first != 2901 ||
            second_source.first != 3001 ||
            throwing_copy_parameter_constructor_evaluations != 2 ||
            throwing_copy_parameter_destructor_evaluations != 2 ||
            throwing_copy_parameter_callee_evaluations != 1) return 94;
    }
    if (throwing_copy_parameter_destructor_evaluations != 4) return 95;

    return 0;
}

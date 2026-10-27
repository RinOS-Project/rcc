/* Aggregate constexpr member and element access coverage. */

struct Pair {
    int first;
    int second;
};

struct Nested {
    Pair pair;
    int tail;
};

struct ConstructorBodyBase {
    int seed;

    constexpr ConstructorBodyBase(int initial) : seed(initial) {}
};

struct ConstructorBodyDerived : ConstructorBodyBase {
    int value;

    constexpr ConstructorBodyDerived(int seed, int initial, int increment)
        : ConstructorBodyBase(seed), value(initial) {
        if (increment >= 0) {
            this->value += increment;
            return;
        }
        this->value -= -increment;
    }
};

struct ConstructorDefaultBase {
    int seed;

    constexpr ConstructorDefaultBase() : seed(19) {}
};

struct ConstructorDefaultMember {
    int value;

    constexpr ConstructorDefaultMember() : value(23) {}
};

struct ConstructorNoBaseConstexpr {
    int value;

    constexpr explicit ConstructorNoBaseConstexpr(int initial)
        : value(initial) {}
};

struct ConstructorDefaultInitializedScalars {
    int before;
    int initialized;
    int after;

    constexpr explicit ConstructorDefaultInitializedScalars(int value)
        : initialized(value) {}
};

struct ConstructorDefaultedSubobjects : ConstructorDefaultBase {
    ConstructorDefaultMember member;
    int value;
    int default_member = 37;

    constexpr explicit ConstructorDefaultedSubobjects(int initial)
        : value(initial) {}
};

struct ConstructorConstReference : ConstructorDefaultBase {
    int value;

    constexpr explicit ConstructorConstReference(const int& initial)
        : value(initial) {}
};

struct ConstructorConstPointerReference : ConstructorDefaultBase {
    const int* value;

    constexpr explicit ConstructorConstPointerReference(
        const int* const& initial)
        : value(initial) {}
};

struct ConstructorMutableScalarReference {
    int observed;

    constexpr explicit ConstructorMutableScalarReference(int& source)
        : observed(source) {
        source += 7;
    }
};

struct ConstructorForwardedMutableScalarReference {
    ConstructorMutableScalarReference nested;

    constexpr explicit ConstructorForwardedMutableScalarReference(
        int& source)
        : nested(source) {}
};

struct ConstructorPointerDereference : ConstructorDefaultBase {
    int value;

    constexpr explicit ConstructorPointerDereference(const int* initial)
        : value(*initial) {}
};

struct ConstructorAggregatePointerBody : ConstructorDefaultBase {
    Pair* alias;

    constexpr explicit ConstructorAggregatePointerBody(Pair* initial)
        : alias(initial) {
        initial->first += 3;
    }
};

struct FloatPair {
    float first;
    double second;
};

constexpr Pair make_pair(int first, int second) {
    return Pair{first, second};
}

constexpr int add_through_mutable_scalar_reference(int& value) {
    value += 5;
    return value;
}

constexpr int forward_mutable_scalar_reference(int& value) {
    return add_through_mutable_scalar_reference(value);
}

constexpr Pair make_pair_after_mutating_scalar_reference(int& value) {
    value += 2;
    return Pair{value, value + 1};
}

constexpr FloatPair make_float_pair(float first, double second) {
    return FloatPair{first, second};
}

consteval FloatPair make_consteval_float_pair(float first, double second) {
    return FloatPair{first, second};
}

constexpr Pair make_mutated_pair(int value) {
    Pair result{value, value + 1};
    result.first += 2;
    ++result.second;
    return result;
}

constexpr Pair pair{3, 4};
constexpr Nested nested{{5, 6}, 7};
constexpr Pair returned_pair = make_pair(11, 13);
constexpr Pair mutated_returned_pair = make_mutated_pair(20);
constexpr ConstructorBodyDerived constructor_body_value{19, 20, 22};
constexpr ConstructorBodyDerived constructor_body_negative_value{
    23, 20, -3};
constexpr ConstructorDefaultedSubobjects constructor_defaulted_subobjects{31};
constexpr ConstructorNoBaseConstexpr constructor_no_base{41};
constexpr int constructor_reference_source = 29;
constexpr ConstructorConstReference constructor_reference_global{
    constructor_reference_source};
constexpr ConstructorConstReference constructor_reference_temporary{31};
constexpr int constructor_pointer_reference_source = 43;
constexpr const int* constructor_pointer_reference_global =
    &constructor_pointer_reference_source;
constexpr ConstructorConstPointerReference
    constructor_pointer_reference_lvalue{
        constructor_pointer_reference_global};
constexpr ConstructorConstPointerReference
    constructor_pointer_reference_temporary{
        &constructor_pointer_reference_source};
constexpr ConstructorPointerDereference constructor_pointer_dereference_lvalue{
    constructor_pointer_reference_global};
constexpr ConstructorPointerDereference
    constructor_pointer_dereference_temporary{
        &constructor_pointer_reference_source};
constexpr FloatPair float_pair{1.25f, 2.5};
constexpr FloatPair returned_float_pair = make_float_pair(1.5f, 2.25);
constexpr FloatPair consteval_float_pair = make_consteval_float_pair(2.5f, 3.75);
constexpr int first_value = pair.first;
constexpr int second_value = pair.second + 1;
constexpr int nested_value = nested.pair.second + nested.tail;
constexpr int returned_value = returned_pair.first + returned_pair.second;
constexpr int mutated_returned_value =
    mutated_returned_pair.first + mutated_returned_pair.second;
constexpr float float_first_value = float_pair.first;
constexpr double float_returned_value =
    returned_float_pair.first + returned_float_pair.second;
constexpr double consteval_float_value =
    consteval_float_pair.first + consteval_float_pair.second;
constexpr int array_values[3] = {8, 9, 10};
constexpr int array_value = array_values[1] + array_values[2];

constexpr int mutate_pair() {
    Pair value{1, 2};
    value.first += 4;
    ++value.second;
    Pair copy = value;
    copy.second += 3;
    value = copy;
    return value.first * 10 + value.second;
}

constexpr int mutate_array() {
    int values[3] = {2, 4, 6};
    values[1] += 5;
    ++values[2];
    return values[0] + values[1] + values[2];
}

constexpr int read_local_constructor_reference() {
    const int initial = 37;
    ConstructorConstReference reference{initial};
    return reference.value;
}

constexpr int mutated_pair_value = mutate_pair();
constexpr int mutated_array_value = mutate_array();
constexpr int constructor_reference_local_value =
    read_local_constructor_reference();

constexpr int mutate_aggregate_through_constructor() {
    Pair source{10, 20};
    ConstructorAggregatePointerBody pointer_mutation{&source};
    return source.first * 100 + source.second + pointer_mutation.alias->first;
}

constexpr int aggregate_pointer_constructor_result =
    mutate_aggregate_through_constructor();

constexpr int mutate_scalar_reference_through_constructor() {
    Pair source{9, 12};
    ConstructorMutableScalarReference alias{source.first};
    return source.first * 100 + alias.observed;
}

constexpr int scalar_reference_constructor_result =
    mutate_scalar_reference_through_constructor();

constexpr int mutate_local_scalar_through_constructor_reference() {
    int source = 10;
    ConstructorMutableScalarReference alias{source};
    return source * 100 + alias.observed;
}

constexpr int local_scalar_reference_constructor_result =
    mutate_local_scalar_through_constructor_reference();

constexpr int mutate_scalar_through_forwarded_constructor_reference() {
    Pair source{11, 12};
    ConstructorForwardedMutableScalarReference alias{source.first};
    return source.first * 100 + alias.nested.observed;
}

constexpr int forwarded_scalar_reference_constructor_result =
    mutate_scalar_through_forwarded_constructor_reference();

constexpr int mutate_local_scalar_through_constexpr_function() {
    int source = 4;
    int result = forward_mutable_scalar_reference(source);
    return source * 100 + result;
}

constexpr int local_scalar_reference_function_result =
    mutate_local_scalar_through_constexpr_function();

constexpr int mutate_aggregate_member_through_constexpr_function() {
    Pair source{6, 12};
    int result = add_through_mutable_scalar_reference(source.first);
    return source.first * 100 + result;
}

constexpr int aggregate_member_reference_function_result =
    mutate_aggregate_member_through_constexpr_function();

constexpr int mutate_scalar_through_aggregate_return_function() {
    int source = 5;
    Pair result = make_pair_after_mutating_scalar_reference(source);
    return source * 100 + result.first * 10 + result.second;
}

constexpr int aggregate_return_reference_function_result =
    mutate_scalar_through_aggregate_return_function();

static_assert(pair.first == 3);
static_assert(nested.pair.second + nested.tail == 13);
static_assert(returned_pair.first == 11 && returned_pair.second == 13);
static_assert(mutated_returned_pair.first == 22 &&
              mutated_returned_pair.second == 22);
static_assert(constructor_body_value.seed == 19 &&
              constructor_body_value.value == 42);
static_assert(constructor_body_negative_value.seed == 23 &&
              constructor_body_negative_value.value == 17);
static_assert(constructor_defaulted_subobjects.seed == 19 &&
              constructor_defaulted_subobjects.member.value == 23 &&
              constructor_defaulted_subobjects.value == 31 &&
              constructor_defaulted_subobjects.default_member == 37);
static_assert(constructor_no_base.value == 41);
static_assert(constructor_reference_global.seed == 19 &&
              constructor_reference_global.value == 29 &&
              constructor_reference_temporary.value == 31);
static_assert(constructor_reference_local_value == 37);
static_assert(constructor_pointer_reference_lvalue.seed == 19 &&
              constructor_pointer_reference_lvalue.value ==
                  &constructor_pointer_reference_source &&
              constructor_pointer_reference_temporary.value ==
                  &constructor_pointer_reference_source);
static_assert(constructor_pointer_dereference_lvalue.seed == 19 &&
              constructor_pointer_dereference_lvalue.value == 43 &&
              constructor_pointer_dereference_temporary.value == 43);
static_assert(aggregate_pointer_constructor_result == 1333);
static_assert(scalar_reference_constructor_result == 1609);
static_assert(local_scalar_reference_constructor_result == 1710);
static_assert(forwarded_scalar_reference_constructor_result == 1811);
static_assert(local_scalar_reference_function_result == 909);
static_assert(aggregate_member_reference_function_result == 1111);
static_assert(aggregate_return_reference_function_result == 778);
static_assert(consteval_float_value == 6.25);
static_assert(array_values[0] + array_values[1] == 17,
              "aggregate constexpr subscript evaluation");
static_assert(mutated_pair_value == 56);
static_assert(mutated_array_value == 18);

int main(void) {
    ConstructorDefaultInitializedScalars default_initialized{17};
    default_initialized.before = 3;
    default_initialized.after = 5;
    Pair runtime_source{9, 12};
    ConstructorMutableScalarReference runtime_alias{runtime_source.first};
    int runtime_scalar_source = 10;
    ConstructorMutableScalarReference runtime_scalar_alias{
        runtime_scalar_source};
    Pair runtime_forwarded_source{11, 12};
    ConstructorForwardedMutableScalarReference runtime_forwarded_alias{
        runtime_forwarded_source.first};
    int runtime_function_source = 4;
    int runtime_function_result =
        forward_mutable_scalar_reference(runtime_function_source);
    Pair runtime_function_aggregate{6, 12};
    int runtime_function_aggregate_result =
        add_through_mutable_scalar_reference(runtime_function_aggregate.first);
    int runtime_aggregate_return_source = 5;
    Pair runtime_aggregate_return_result =
        make_pair_after_mutating_scalar_reference(
            runtime_aggregate_return_source);
    return first_value == 3 && second_value == 5 &&
                   nested_value == 13 && array_value == 19 &&
                   returned_value == 24 && mutated_returned_value == 44 &&
                   constructor_body_value.seed == 19 &&
                   constructor_body_value.value == 42 &&
                   constructor_body_negative_value.seed == 23 &&
                   constructor_body_negative_value.value == 17 &&
                   constructor_defaulted_subobjects.seed == 19 &&
                   constructor_defaulted_subobjects.member.value == 23 &&
                   constructor_defaulted_subobjects.value == 31 &&
                   constructor_defaulted_subobjects.default_member == 37 &&
                   constructor_no_base.value == 41 &&
                   default_initialized.before +
                           default_initialized.initialized +
                           default_initialized.after == 25 &&
                   constructor_reference_global.seed == 19 &&
                   constructor_reference_global.value == 29 &&
                   constructor_reference_temporary.value == 31 &&
                   constructor_reference_local_value == 37 &&
                   constructor_pointer_reference_lvalue.seed == 19 &&
                   constructor_pointer_reference_lvalue.value ==
                       &constructor_pointer_reference_source &&
                   constructor_pointer_reference_temporary.value ==
                       &constructor_pointer_reference_source &&
                   constructor_pointer_dereference_lvalue.seed == 19 &&
                   constructor_pointer_dereference_lvalue.value == 43 &&
                   constructor_pointer_dereference_temporary.value == 43 &&
                   aggregate_pointer_constructor_result == 1333 &&
                   scalar_reference_constructor_result == 1609 &&
                   local_scalar_reference_constructor_result == 1710 &&
                   runtime_source.first == 16 &&
                   runtime_alias.observed == 9 &&
                   runtime_scalar_source == 17 &&
                   runtime_scalar_alias.observed == 10 &&
                   runtime_forwarded_source.first == 18 &&
                   runtime_forwarded_alias.nested.observed == 11 &&
                   runtime_function_source == 9 &&
                   runtime_function_result == 9 &&
                   runtime_function_aggregate.first == 11 &&
                   runtime_function_aggregate_result == 11 &&
                   runtime_aggregate_return_source == 7 &&
                   runtime_aggregate_return_result.first == 7 &&
                   runtime_aggregate_return_result.second == 8 &&
                   float_first_value == 1.25f &&
                   float_returned_value == 3.75 &&
                   consteval_float_value == 6.25 &&
                   mutated_pair_value == 56 && mutated_array_value == 18
               ? 0 : 1;
}

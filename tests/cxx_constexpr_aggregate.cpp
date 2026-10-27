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
static_assert(consteval_float_value == 6.25);
static_assert(array_values[0] + array_values[1] == 17,
              "aggregate constexpr subscript evaluation");
static_assert(mutated_pair_value == 56);
static_assert(mutated_array_value == 18);

int main(void) {
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
                   float_first_value == 1.25f &&
                   float_returned_value == 3.75 &&
                   consteval_float_value == 6.25 &&
                   mutated_pair_value == 56 && mutated_array_value == 18
               ? 0 : 1;
}

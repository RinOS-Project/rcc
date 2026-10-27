/* Aggregate constexpr member and element access coverage. */

struct Pair {
    int first;
    int second;
};

struct Nested {
    Pair pair;
    int tail;
};

union MutableReferenceUnion {
    int value;
    int other;
};

union MutableAggregateReferenceUnion {
    Pair pair;
    int scalar;
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

struct ConstructorBodyInitializesOmitted {
    int written_by_body;
    int initialized;

    constexpr explicit ConstructorBodyInitializesOmitted(int initial)
        : initialized(initial) {
        this->written_by_body = initial + 1;
    }
};

struct ConstructorBodyReadsInitialized {
    int initialized;
    int written_by_body;

    constexpr explicit ConstructorBodyReadsInitialized(int initial)
        : initialized(initial) {
        this->written_by_body = this->initialized + 1;
    }
};

struct ConstructorBodyReadsPriorWrite {
    int written_first;
    int written_second;

    constexpr explicit ConstructorBodyReadsPriorWrite(int initial) {
        this->written_first = initial;
        this->written_second = this->written_first + 1;
    }
};

struct ConstructorBodyBranchesInitialize {
    int value;

    constexpr explicit ConstructorBodyBranchesInitialize(int initial) {
        if (initial >= 0) {
            this->value = initial;
        } else {
            this->value = -initial;
        }
    }
};

struct ConstructorBodySelectedBranchInitializes {
    int value;

    constexpr explicit ConstructorBodySelectedBranchInitializes(int initial) {
        if (initial > 0) {
            this->value = initial;
        }
    }
};

struct ConstructorBodyReadsDefaultMember {
    int initialized = 61;
    int written_by_body;

    constexpr ConstructorBodyReadsDefaultMember() {
        this->written_by_body = this->initialized + 1;
    }
};

struct ConstructorBodyPostIncrement {
    int source;
    int result;

    constexpr explicit ConstructorBodyPostIncrement(int initial)
        : source(initial) {
        this->result = this->source++;
    }
};

struct ConstructorBodyCompoundAssignments {
    int accumulator = 5;
    int result;

    constexpr explicit ConstructorBodyCompoundAssignments(int amount) {
        this->accumulator += amount;
        this->accumulator -= 2;
        this->accumulator *= 2;
        this->accumulator /= 3;
        this->accumulator %= 3;
        this->accumulator |= 8;
        this->accumulator &= 15;
        this->accumulator ^= 2;
        this->accumulator <<= 1;
        this->accumulator >>= 1;
        this->accumulator++;
        ++this->accumulator;
        this->accumulator--;
        --this->accumulator;
        this->result = this->accumulator;
    }
};

struct ConstructorBodySelectedRhsBranches {
    int delayed;
    int conditional;
    int short_and;
    int short_or;

    constexpr explicit ConstructorBodySelectedRhsBranches(bool choose) {
        this->conditional = choose ? 47 : this->delayed;
        this->short_and = !choose && this->delayed;
        this->short_or = choose || this->delayed;
        this->delayed = 53;
    }
};

struct ConstructorBodyInitializedLocals {
    int value;
    int doubled;

    constexpr explicit ConstructorBodyInitializedLocals(int initial) {
        int adjusted;
        adjusted = initial;
        adjusted = adjusted + 1;
        adjusted += 2;
        adjusted++;
        --adjusted;
        this->value = adjusted;
        {
            const int twice = adjusted * 2;
            this->doubled = twice;
        }
    }
};

struct ConstructorBodySelectedLocalMutation {
    int value;
    int doubled;

    constexpr explicit ConstructorBodySelectedLocalMutation(
        bool choose_first, int initial) {
        int temporary;
        if (choose_first) {
            temporary = initial + 2;
        } else {
            temporary = initial - 2;
        }
        if constexpr (true) {
            temporary += 3;
        }
        this->value = temporary;
        this->doubled = temporary * 2;
    }
};

struct ConstructorBodyForLoop {
    int value;

    constexpr explicit ConstructorBodyForLoop(int count, int base) {
        int total = 0;
        for (int index = 0; index < count; ++index) {
            total += base + index;
        }
        this->value = total;
    }
};

struct ConstructorBodyForLoopMemberWrite {
    int value;

    constexpr explicit ConstructorBodyForLoopMemberWrite(
        int count, int first) {
        for (int index = 0; index < count; ++index) {
            this->value = first + index;
        }
    }
};

struct ConstructorBodyWhileLoop {
    int value;

    constexpr explicit ConstructorBodyWhileLoop(int count, int base) {
        int index = 0;
        int total = 0;
        while (index < count) {
            total += base + index;
            ++index;
        }
        this->value = total;
    }
};

struct ConstructorBodyDoWhileLoop {
    int value;

    constexpr explicit ConstructorBodyDoWhileLoop(int count, int base) {
        int index = 0;
        int total = 0;
        do {
            total += base + index;
            ++index;
        } while (index < count);
        this->value = total;
    }
};

struct ConstructorBodyWhileLoopMemberWrite {
    int value;

    constexpr explicit ConstructorBodyWhileLoopMemberWrite(int count) {
        int index = 0;
        while (index < count) {
            this->value = 100 + index;
            ++index;
        }
    }
};

struct ConstructorBodyDoWhileLoopMemberWrite {
    int value;

    constexpr explicit ConstructorBodyDoWhileLoopMemberWrite(int count) {
        int index = 0;
        do {
            this->value = 200 + index;
            ++index;
        } while (index < count);
    }
};

struct ConstructorBodyLoopControl {
    int for_value;
    int while_value;
    int do_while_value;

    constexpr explicit ConstructorBodyLoopControl(int limit) {
        int for_total = 0;
        for (int index = 0; index < limit; ++index) {
            if (index == 1) continue;
            if (index == 4) break;
            for_total += index;
        }
        this->for_value = for_total;

        int while_index = 0;
        int while_total = 0;
        while (while_index < limit) {
            ++while_index;
            if (while_index == 2) continue;
            if (while_index == 5) break;
            while_total += while_index;
        }
        this->while_value = while_total;

        int do_while_index = 0;
        int do_while_total = 0;
        do {
            ++do_while_index;
            if (do_while_index == 2) continue;
            if (do_while_index == 5) break;
            do_while_total += do_while_index;
        } while (do_while_index < limit);
        this->do_while_value = do_while_total;
    }
};

struct ConstructorBodySelectedReturn {
    int value;

    constexpr explicit ConstructorBodySelectedReturn(bool finish_early,
                                                      int initial) {
        if (finish_early) {
            this->value = initial;
            return;
        }
        this->value = initial + 1;
    }
};

struct ConstructorBodySwitch {
    int value;

    constexpr explicit ConstructorBodySwitch(int selector, int initial) {
        switch (selector) {
            case 0:
                this->value = initial;
                /* fall through */
            case 1:
                if (selector == 0) {
                    this->value += 2;
                } else {
                    this->value = initial + 10;
                }
                break;
            default:
                this->value = initial + 20;
                break;
        }
    }
};

struct ConstructorDefaultInitializedScalars {
    int before;
    int initialized;
    int after;

    constexpr explicit ConstructorDefaultInitializedScalars(int value)
        : initialized(value) {}
};

struct ImplicitDefaultNested {
    int first;
    int second;
};

struct ConstructorImplicitDefaultNested {
    ImplicitDefaultNested nested;
    int initialized;

    constexpr explicit ConstructorImplicitDefaultNested(int value)
        : initialized(value) {}
};

struct ConstructorImplicitDefaultArray {
    int values[2];
    int initialized;

    constexpr explicit ConstructorImplicitDefaultArray(int value)
        : initialized(value) {}
};

struct ImplicitDefaultBase {
    int value;
};

struct ConstructorImplicitDefaultBase : ImplicitDefaultBase {
    int initialized;

    constexpr explicit ConstructorImplicitDefaultBase(int value)
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

struct ConstructorMutableAggregateReference {
    int observed;

    constexpr explicit ConstructorMutableAggregateReference(Pair& source)
        : observed(source.first) {
        source.first += 2;
    }
};

struct ConstructorForwardedMutableAggregateReference {
    ConstructorMutableAggregateReference nested;

    constexpr explicit ConstructorForwardedMutableAggregateReference(
        Pair& source)
        : nested(source) {}
};

struct ConstructorMutableArrayReference {
    int observed;

    constexpr explicit ConstructorMutableArrayReference(int (&source)[2])
        : observed(source[0]) {
        source[0] += 2;
    }
};

struct ConstructorMutableUnionReference {
    int observed;

    constexpr explicit ConstructorMutableUnionReference(
        MutableReferenceUnion& source)
        : observed(source.value) {
        source.value += 2;
    }
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

constexpr int add_through_mutable_aggregate_reference(Pair& value) {
    value.first += 3;
    return value.first * 100 + value.second;
}

constexpr int forward_mutable_aggregate_reference(Pair& value) {
    return add_through_mutable_aggregate_reference(value);
}

constexpr int add_through_mutable_array_reference(int (&values)[2]) {
    values[0] += 3;
    return values[0] * 100 + values[1];
}

constexpr int add_through_mutable_union_reference(
    MutableReferenceUnion& value) {
    value.value += 2;
    return value.value;
}

constexpr int switch_mutable_union_reference_member(
    MutableReferenceUnion& value) {
    value.other = value.value + 3;
    return value.other;
}

constexpr Pair make_pair_after_mutating_union_reference(
    MutableReferenceUnion& value) {
    value.value += 2;
    return Pair{value.value, value.value + 1};
}

constexpr int assign_through_mutable_union_references(
    MutableReferenceUnion& target, MutableReferenceUnion& source) {
    target = source;
    target.value += 1;
    return target.value;
}

constexpr int mutate_union_aggregate_alternative(
    MutableAggregateReferenceUnion& value) {
    value.pair.first += 2;
    value.pair.second += 3;
    return value.pair.first * 100 + value.pair.second;
}

constexpr int transition_union_aggregate_alternative(
    MutableAggregateReferenceUnion& value) {
    value.scalar = value.pair.first + value.pair.second;
    value.pair.first = value.scalar + 1;
    value.pair.second = value.pair.first + 2;
    return value.pair.first * 100 + value.pair.second;
}

constexpr Pair make_pair_after_mutating_scalar_reference(int& value) {
    value += 2;
    return Pair{value, value + 1};
}

constexpr Pair make_pair_after_mutating_aggregate_reference(Pair& value) {
    value.first += 2;
    return Pair{value.first, value.second};
}

constexpr Pair make_pair_after_mutating_array_reference(int (&values)[2]) {
    values[1] += 4;
    return Pair{values[0], values[1]};
}

constexpr int assign_through_mutable_aggregate_references(Pair& target,
                                                          Pair& source) {
    target = source;
    target.second += 1;
    return target.first * 100 + target.second;
}

constexpr int redirect_mutable_pointer_reference(int*& target,
                                                  int* replacement) {
    target = replacement;
    return *target;
}

constexpr int redirect_mutable_aggregate_pointer_reference(
    Pair*& target, Pair* replacement) {
    target = replacement;
    target->first += 3;
    return target->first * 100 + target->second;
}

constexpr int redirect_mutable_const_pointer_reference(
    const int*& target, const int* replacement) {
    target = replacement;
    return *target;
}

constexpr int redirect_mutable_double_pointer_reference(
    int**& target, int** replacement) {
    target = replacement;
    **target += 4;
    return **target;
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
constexpr ConstructorBodyInitializesOmitted
    constructor_body_initializes_omitted{43};
constexpr ConstructorBodyReadsInitialized
    constructor_body_reads_initialized{53};
constexpr ConstructorBodyReadsPriorWrite constructor_body_reads_prior_write{
    67};
constexpr ConstructorBodyBranchesInitialize
    constructor_body_branches_initialize{83};
constexpr ConstructorBodySelectedBranchInitializes
    constructor_body_selected_branch_initializes{97};
constexpr ConstructorBodyReadsDefaultMember
    constructor_body_reads_default_member{};
constexpr ConstructorBodyPostIncrement constructor_body_post_increment{107};
constexpr ConstructorBodyCompoundAssignments
    constructor_body_compound_assignments{4};
constexpr ConstructorBodySelectedRhsBranches
    constructor_body_selected_rhs_branches{true};
constexpr ConstructorBodyInitializedLocals
    constructor_body_initialized_locals{59};
constexpr ConstructorBodySelectedLocalMutation
    constructor_body_selected_local_mutation_true{true, 31};
constexpr ConstructorBodySelectedLocalMutation
    constructor_body_selected_local_mutation_false{false, 41};
constexpr ConstructorBodyForLoop constructor_body_for_loop_empty{0, 3};
constexpr ConstructorBodyForLoop constructor_body_for_loop_iterations{4, 3};
constexpr ConstructorBodyForLoopMemberWrite
    constructor_body_for_loop_member_write{3, 10};
constexpr ConstructorBodyWhileLoop constructor_body_while_loop_empty{0, 3};
constexpr ConstructorBodyWhileLoop constructor_body_while_loop_iterations{
    4, 3};
constexpr ConstructorBodyDoWhileLoop constructor_body_do_while_loop_empty{
    0, 7};
constexpr ConstructorBodyDoWhileLoop
    constructor_body_do_while_loop_iterations{3, 7};
constexpr ConstructorBodyWhileLoopMemberWrite
    constructor_body_while_loop_member_write{3};
constexpr ConstructorBodyDoWhileLoopMemberWrite
    constructor_body_do_while_loop_member_write{0};
constexpr ConstructorBodyLoopControl constructor_body_loop_control{6};
constexpr ConstructorBodySelectedReturn constructor_body_return_early{
    true, 23};
constexpr ConstructorBodySelectedReturn constructor_body_return_fallthrough{
    false, 23};
constexpr ConstructorBodySwitch constructor_body_switch_fallthrough{0, 40};
constexpr ConstructorBodySwitch constructor_body_switch_case{1, 40};
constexpr ConstructorBodySwitch constructor_body_switch_default{7, 40};
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

constexpr int mutate_aggregate_through_forwarded_constructor_reference() {
    Pair source{3, 8};
    ConstructorForwardedMutableAggregateReference alias{source};
    return source.first * 100 + alias.nested.observed;
}

constexpr int forwarded_aggregate_reference_constructor_result =
    mutate_aggregate_through_forwarded_constructor_reference();

constexpr int mutate_array_through_constructor_reference() {
    int source[2] = {3, 8};
    ConstructorMutableArrayReference alias{source};
    return source[0] * 100 + alias.observed;
}

constexpr int array_reference_constructor_result =
    mutate_array_through_constructor_reference();

constexpr int mutate_union_through_constructor_reference() {
    MutableReferenceUnion source{3};
    ConstructorMutableUnionReference alias{source};
    return source.value * 100 + alias.observed;
}

constexpr int union_reference_constructor_result =
    mutate_union_through_constructor_reference();

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

constexpr int mutate_aggregate_through_constexpr_function() {
    Pair source{4, 8};
    int result = forward_mutable_aggregate_reference(source);
    return source.first * 1000 + result;
}

constexpr int aggregate_object_reference_function_result =
    mutate_aggregate_through_constexpr_function();

constexpr int mutate_array_through_constexpr_function() {
    int source[2] = {4, 8};
    int result = add_through_mutable_array_reference(source);
    return source[0] * 1000 + result;
}

constexpr int array_reference_function_result =
    mutate_array_through_constexpr_function();

constexpr int mutate_union_through_constexpr_function() {
    MutableReferenceUnion source{4};
    int result = add_through_mutable_union_reference(source);
    return source.value * 100 + result;
}

constexpr int union_reference_function_result =
    mutate_union_through_constexpr_function();

constexpr int switch_union_member_through_constexpr_function() {
    MutableReferenceUnion source{4};
    int result = switch_mutable_union_reference_member(source);
    return source.other * 100 + result;
}

constexpr int union_reference_member_switch_result =
    switch_union_member_through_constexpr_function();

constexpr int mutate_union_through_aggregate_return_function() {
    MutableReferenceUnion source{5};
    Pair result = make_pair_after_mutating_union_reference(source);
    return source.value * 1000 + result.first * 10 + result.second;
}

constexpr int aggregate_return_union_reference_result =
    mutate_union_through_aggregate_return_function();

constexpr int assign_union_through_reference_function() {
    MutableReferenceUnion target{1};
    MutableReferenceUnion source{6};
    int result = assign_through_mutable_union_references(target, source);
    return target.value * 100 + result;
}

constexpr int union_reference_assignment_result =
    assign_union_through_reference_function();

constexpr int mutate_union_aggregate_alternative_function() {
    MutableAggregateReferenceUnion source{Pair{1, 2}};
    int result = mutate_union_aggregate_alternative(source);
    return source.pair.first * 1000 + source.pair.second * 10 + result;
}

constexpr int union_aggregate_reference_result =
    mutate_union_aggregate_alternative_function();

constexpr int transition_union_aggregate_alternative_function() {
    MutableAggregateReferenceUnion source{Pair{3, 4}};
    int result = transition_union_aggregate_alternative(source);
    return source.pair.first * 1000 + source.pair.second * 10 + result;
}

constexpr int union_aggregate_member_transition_result =
    transition_union_aggregate_alternative_function();

constexpr int mutate_scalar_through_aggregate_return_function() {
    int source = 5;
    Pair result = make_pair_after_mutating_scalar_reference(source);
    return source * 100 + result.first * 10 + result.second;
}

constexpr int aggregate_return_reference_function_result =
    mutate_scalar_through_aggregate_return_function();

constexpr int mutate_aggregate_through_aggregate_return_function() {
    Pair source{5, 8};
    Pair result = make_pair_after_mutating_aggregate_reference(source);
    return source.first * 1000 + result.first * 10 + result.second;
}

constexpr int aggregate_return_object_reference_function_result =
    mutate_aggregate_through_aggregate_return_function();

constexpr int mutate_array_through_aggregate_return_function() {
    int source[2] = {5, 8};
    Pair result = make_pair_after_mutating_array_reference(source);
    return source[1] * 1000 + result.first * 10 + result.second;
}

constexpr int aggregate_return_array_reference_function_result =
    mutate_array_through_aggregate_return_function();

constexpr int assign_through_aggregate_reference_function() {
    Pair target{1, 2};
    Pair source{6, 9};
    int result = assign_through_mutable_aggregate_references(target, source);
    return target.first * 1000 + target.second * 10 + result;
}

constexpr int aggregate_reference_assignment_function_result =
    assign_through_aggregate_reference_function();

constexpr int redirect_pointer_through_constexpr_function() {
    Pair source{2, 7};
    int* target = &source.first;
    int* replacement = &source.second;
    int result = redirect_mutable_pointer_reference(target, replacement);
    return source.first * 100 + source.second * 10 + result;
}

constexpr int mutable_pointer_reference_function_result =
    redirect_pointer_through_constexpr_function();

constexpr int redirect_aggregate_pointer_through_constexpr_function() {
    Pair original{1, 2};
    Pair replacement{4, 8};
    Pair* target = &original;
    Pair* next = &replacement;
    int result = redirect_mutable_aggregate_pointer_reference(target, next);
    return original.first * 1000 + replacement.first * 100 + result;
}

constexpr int mutable_aggregate_pointer_reference_function_result =
    redirect_aggregate_pointer_through_constexpr_function();

constexpr int redirect_const_pointer_through_constexpr_function() {
    int original = 3;
    int replacement = 9;
    const int* target = &original;
    const int* next = &replacement;
    int result = redirect_mutable_const_pointer_reference(target, next);
    return original * 100 + replacement * 10 + result;
}

constexpr int mutable_const_pointer_reference_function_result =
    redirect_const_pointer_through_constexpr_function();

constexpr int dereference_local_double_pointer() {
    int value = 9;
    int* pointer = &value;
    int** double_pointer = &pointer;
    return **double_pointer;
}

constexpr int local_double_pointer_dereference_result =
    dereference_local_double_pointer();

constexpr int redirect_double_pointer_through_constexpr_function() {
    int original = 3;
    int replacement = 6;
    int* original_pointer = &original;
    int* replacement_pointer = &replacement;
    int** target = &original_pointer;
    int** next = &replacement_pointer;
    int result = redirect_mutable_double_pointer_reference(target, next);
    return original * 1000 + replacement * 100 + result * 10 +
           (target == next ? 1 : 0);
}

constexpr int mutable_double_pointer_reference_function_result =
    redirect_double_pointer_through_constexpr_function();

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
static_assert(forwarded_aggregate_reference_constructor_result == 503);
static_assert(array_reference_constructor_result == 503);
static_assert(union_reference_constructor_result == 503);
static_assert(local_scalar_reference_function_result == 909);
static_assert(aggregate_member_reference_function_result == 1111);
static_assert(aggregate_object_reference_function_result == 7708);
static_assert(array_reference_function_result == 7708);
static_assert(union_reference_function_result == 606);
static_assert(union_reference_member_switch_result == 707);
static_assert(aggregate_return_union_reference_result == 7078);
static_assert(union_reference_assignment_result == 707);
static_assert(union_aggregate_reference_result == 3355);
static_assert(union_aggregate_member_transition_result == 8910);
static_assert(aggregate_return_reference_function_result == 778);
static_assert(aggregate_return_object_reference_function_result == 7078);
static_assert(aggregate_return_array_reference_function_result == 12062);
static_assert(aggregate_reference_assignment_function_result == 6710);
static_assert(mutable_pointer_reference_function_result == 277);
static_assert(mutable_aggregate_pointer_reference_function_result == 2408);
static_assert(mutable_const_pointer_reference_function_result == 399);
static_assert(local_double_pointer_dereference_result == 9);
static_assert(mutable_double_pointer_reference_function_result == 4101);
static_assert(consteval_float_value == 6.25);
static_assert(array_values[0] + array_values[1] == 17,
              "aggregate constexpr subscript evaluation");
static_assert(mutated_pair_value == 56);
static_assert(mutated_array_value == 18);

int main(void) {
    ConstructorDefaultInitializedScalars default_initialized{17};
    default_initialized.before = 3;
    default_initialized.after = 5;
    ConstructorBodyInitializesOmitted runtime_body_initializes_omitted{47};
    ConstructorBodyReadsInitialized runtime_body_reads_initialized{59};
    ConstructorBodyReadsPriorWrite runtime_body_reads_prior_write{73};
    ConstructorBodyBranchesInitialize runtime_body_branches_initialize{-89};
    ConstructorBodySelectedBranchInitializes
        runtime_body_selected_branch_initializes{101};
    ConstructorBodyReadsDefaultMember runtime_body_reads_default_member{};
    ConstructorBodyPostIncrement runtime_body_post_increment{109};
    ConstructorBodyCompoundAssignments runtime_body_compound_assignments{4};
    ConstructorBodySelectedRhsBranches runtime_body_selected_rhs_branches{true};
    ConstructorBodyInitializedLocals runtime_body_initialized_locals{61};
    ConstructorBodySelectedLocalMutation runtime_body_selected_local_true{
        true, 33};
    ConstructorBodySelectedLocalMutation runtime_body_selected_local_false{
        false, 45};
    ConstructorBodyForLoop runtime_body_for_loop_empty{0, 9};
    ConstructorBodyForLoop runtime_body_for_loop_iterations{5, 2};
    ConstructorBodyForLoopMemberWrite runtime_body_for_loop_member_write{
        4, 12};
    ConstructorBodyWhileLoop runtime_body_while_loop_empty{0, 9};
    ConstructorBodyWhileLoop runtime_body_while_loop_iterations{5, 2};
    ConstructorBodyDoWhileLoop runtime_body_do_while_loop_empty{0, 5};
    ConstructorBodyDoWhileLoop runtime_body_do_while_loop_iterations{
        4, 4};
    ConstructorBodyWhileLoopMemberWrite
        runtime_body_while_loop_member_write{4};
    ConstructorBodyDoWhileLoopMemberWrite
        runtime_body_do_while_loop_member_write{0};
    ConstructorBodyLoopControl runtime_body_loop_control{3};
    ConstructorBodySelectedReturn runtime_body_return_early{true, 29};
    ConstructorBodySelectedReturn runtime_body_return_fallthrough{false, 29};
    ConstructorBodySwitch runtime_body_switch_fallthrough{0, 11};
    ConstructorBodySwitch runtime_body_switch_case{1, 11};
    ConstructorBodySwitch runtime_body_switch_default{9, 11};
    ConstructorImplicitDefaultNested implicit_default_nested{29};
    ConstructorImplicitDefaultArray implicit_default_array{31};
    ConstructorImplicitDefaultBase implicit_default_base{37};
    Pair runtime_source{9, 12};
    ConstructorMutableScalarReference runtime_alias{runtime_source.first};
    int runtime_scalar_source = 10;
    ConstructorMutableScalarReference runtime_scalar_alias{
        runtime_scalar_source};
    Pair runtime_forwarded_source{11, 12};
    ConstructorForwardedMutableScalarReference runtime_forwarded_alias{
        runtime_forwarded_source.first};
    Pair runtime_forwarded_aggregate_source{3, 8};
    ConstructorForwardedMutableAggregateReference
        runtime_forwarded_aggregate_alias{runtime_forwarded_aggregate_source};
    int runtime_constructor_array_source[2] = {3, 8};
    ConstructorMutableArrayReference runtime_constructor_array_alias{
        runtime_constructor_array_source};
    MutableReferenceUnion runtime_constructor_union_source{3};
    ConstructorMutableUnionReference runtime_constructor_union_alias{
        runtime_constructor_union_source};
    int runtime_function_source = 4;
    int runtime_function_result =
        forward_mutable_scalar_reference(runtime_function_source);
    Pair runtime_function_aggregate{6, 12};
    int runtime_function_aggregate_result =
        add_through_mutable_scalar_reference(runtime_function_aggregate.first);
    Pair runtime_aggregate_object{4, 8};
    int runtime_aggregate_object_result =
        forward_mutable_aggregate_reference(runtime_aggregate_object);
    int runtime_array_source[2] = {4, 8};
    int runtime_array_result = add_through_mutable_array_reference(
        runtime_array_source);
    MutableReferenceUnion runtime_union_source{4};
    int runtime_union_result =
        add_through_mutable_union_reference(runtime_union_source);
    MutableReferenceUnion runtime_union_switch_source{4};
    int runtime_union_switch_result =
        switch_mutable_union_reference_member(runtime_union_switch_source);
    MutableReferenceUnion runtime_union_return_source{5};
    Pair runtime_union_return_result =
        make_pair_after_mutating_union_reference(runtime_union_return_source);
    MutableReferenceUnion runtime_union_assignment_target{1};
    MutableReferenceUnion runtime_union_assignment_source{6};
    int runtime_union_assignment_result =
        assign_through_mutable_union_references(
            runtime_union_assignment_target, runtime_union_assignment_source);
    MutableAggregateReferenceUnion runtime_union_aggregate_source{
        Pair{1, 2}};
    int runtime_union_aggregate_result =
        mutate_union_aggregate_alternative(runtime_union_aggregate_source);
    MutableAggregateReferenceUnion runtime_union_transition_source{
        Pair{3, 4}};
    int runtime_union_transition_result =
        transition_union_aggregate_alternative(
            runtime_union_transition_source);
    int runtime_aggregate_return_source = 5;
    Pair runtime_aggregate_return_result =
        make_pair_after_mutating_scalar_reference(
            runtime_aggregate_return_source);
    Pair runtime_aggregate_return_object_source{5, 8};
    Pair runtime_aggregate_return_object_result =
        make_pair_after_mutating_aggregate_reference(
            runtime_aggregate_return_object_source);
    int runtime_aggregate_return_array_source[2] = {5, 8};
    Pair runtime_aggregate_return_array_result =
        make_pair_after_mutating_array_reference(
            runtime_aggregate_return_array_source);
    Pair runtime_assignment_target{1, 2};
    Pair runtime_assignment_source{6, 9};
    int runtime_assignment_result = assign_through_mutable_aggregate_references(
        runtime_assignment_target, runtime_assignment_source);
    Pair runtime_pointer_pair{2, 7};
    int* runtime_pointer_target = &runtime_pointer_pair.first;
    int* runtime_pointer_replacement = &runtime_pointer_pair.second;
    int runtime_pointer_result = redirect_mutable_pointer_reference(
        runtime_pointer_target, runtime_pointer_replacement);
    Pair runtime_aggregate_pointer_original{1, 2};
    Pair runtime_aggregate_pointer_replacement{4, 8};
    Pair* runtime_aggregate_pointer_target = &runtime_aggregate_pointer_original;
    Pair* runtime_aggregate_pointer_next = &runtime_aggregate_pointer_replacement;
    int runtime_aggregate_pointer_result =
        redirect_mutable_aggregate_pointer_reference(
            runtime_aggregate_pointer_target, runtime_aggregate_pointer_next);
    int runtime_const_pointer_original = 3;
    int runtime_const_pointer_replacement = 9;
    const int* runtime_const_pointer_target = &runtime_const_pointer_original;
    const int* runtime_const_pointer_next = &runtime_const_pointer_replacement;
    int runtime_const_pointer_result = redirect_mutable_const_pointer_reference(
        runtime_const_pointer_target, runtime_const_pointer_next);
    int runtime_double_pointer_original = 3;
    int runtime_double_pointer_replacement = 6;
    int* runtime_double_pointer_original_pointer =
        &runtime_double_pointer_original;
    int* runtime_double_pointer_replacement_pointer =
        &runtime_double_pointer_replacement;
    int** runtime_double_pointer_target =
        &runtime_double_pointer_original_pointer;
    int** runtime_double_pointer_next =
        &runtime_double_pointer_replacement_pointer;
    int runtime_double_pointer_result =
        redirect_mutable_double_pointer_reference(
            runtime_double_pointer_target, runtime_double_pointer_next);
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
                     constructor_body_initializes_omitted.written_by_body ==
                         44 &&
                     constructor_body_initializes_omitted.initialized == 43 &&
                     constructor_body_reads_initialized.initialized == 53 &&
                     constructor_body_reads_initialized.written_by_body ==
                         54 &&
                     constructor_body_reads_prior_write.written_first ==
                         67 &&
                     constructor_body_reads_prior_write.written_second ==
                         68 &&
                     constructor_body_branches_initialize.value == 83 &&
                     constructor_body_selected_branch_initializes.value ==
                         97 &&
                     constructor_body_reads_default_member.initialized == 61 &&
                     constructor_body_reads_default_member.written_by_body ==
                         62 &&
                     constructor_body_post_increment.source == 108 &&
                     constructor_body_post_increment.result == 107 &&
                     constructor_body_compound_assignments.accumulator == 11 &&
                     constructor_body_compound_assignments.result == 11 &&
                     constructor_body_selected_rhs_branches.delayed == 53 &&
                     constructor_body_selected_rhs_branches.conditional == 47 &&
                     constructor_body_selected_rhs_branches.short_and == 0 &&
                     constructor_body_selected_rhs_branches.short_or == 1 &&
                     constructor_body_initialized_locals.value == 62 &&
                     constructor_body_initialized_locals.doubled == 124 &&
                     constructor_body_selected_local_mutation_true.value == 36 &&
                     constructor_body_selected_local_mutation_true.doubled == 72 &&
                     constructor_body_selected_local_mutation_false.value == 42 &&
                     constructor_body_selected_local_mutation_false.doubled == 84 &&
                     constructor_body_for_loop_empty.value == 0 &&
                     constructor_body_for_loop_iterations.value == 18 &&
                     constructor_body_for_loop_member_write.value == 12 &&
                     constructor_body_while_loop_empty.value == 0 &&
                     constructor_body_while_loop_iterations.value == 18 &&
                     constructor_body_do_while_loop_empty.value == 7 &&
                     constructor_body_do_while_loop_iterations.value == 24 &&
                     constructor_body_while_loop_member_write.value == 102 &&
                     constructor_body_do_while_loop_member_write.value == 200 &&
                     constructor_body_loop_control.for_value == 5 &&
                     constructor_body_loop_control.while_value == 8 &&
                     constructor_body_loop_control.do_while_value == 8 &&
                     constructor_body_return_early.value == 23 &&
                     constructor_body_return_fallthrough.value == 24 &&
                     constructor_body_switch_fallthrough.value == 42 &&
                     constructor_body_switch_case.value == 50 &&
                     constructor_body_switch_default.value == 60 &&
                     runtime_body_initializes_omitted.written_by_body == 48 &&
                     runtime_body_initializes_omitted.initialized == 47 &&
                     runtime_body_reads_initialized.initialized == 59 &&
                     runtime_body_reads_initialized.written_by_body ==
                         60 &&
                     runtime_body_reads_prior_write.written_first == 73 &&
                     runtime_body_reads_prior_write.written_second == 74 &&
                     runtime_body_branches_initialize.value == 89 &&
                     runtime_body_selected_branch_initializes.value == 101 &&
                     runtime_body_reads_default_member.initialized == 61 &&
                     runtime_body_reads_default_member.written_by_body ==
                         62 &&
                     runtime_body_post_increment.source == 110 &&
                     runtime_body_post_increment.result == 109 &&
                     runtime_body_compound_assignments.accumulator == 11 &&
                     runtime_body_compound_assignments.result == 11 &&
                     runtime_body_selected_rhs_branches.delayed == 53 &&
                     runtime_body_selected_rhs_branches.conditional == 47 &&
                     runtime_body_selected_rhs_branches.short_and == 0 &&
                     runtime_body_selected_rhs_branches.short_or == 1 &&
                     runtime_body_initialized_locals.value == 64 &&
                     runtime_body_initialized_locals.doubled == 128 &&
                     runtime_body_selected_local_true.value == 38 &&
                     runtime_body_selected_local_true.doubled == 76 &&
                     runtime_body_selected_local_false.value == 46 &&
                     runtime_body_selected_local_false.doubled == 92 &&
                     runtime_body_for_loop_empty.value == 0 &&
                     runtime_body_for_loop_iterations.value == 20 &&
                     runtime_body_for_loop_member_write.value == 15 &&
                     runtime_body_while_loop_empty.value == 0 &&
                     runtime_body_while_loop_iterations.value == 20 &&
                     runtime_body_do_while_loop_empty.value == 5 &&
                     runtime_body_do_while_loop_iterations.value == 22 &&
                     runtime_body_while_loop_member_write.value == 103 &&
                     runtime_body_do_while_loop_member_write.value == 200 &&
                     runtime_body_loop_control.for_value == 2 &&
                     runtime_body_loop_control.while_value == 4 &&
                     runtime_body_loop_control.do_while_value == 4 &&
                     runtime_body_return_early.value == 29 &&
                     runtime_body_return_fallthrough.value == 30 &&
                     runtime_body_switch_fallthrough.value == 13 &&
                     runtime_body_switch_case.value == 21 &&
                     runtime_body_switch_default.value == 31 &&
                   default_initialized.before +
                           default_initialized.initialized +
                           default_initialized.after == 25 &&
                   implicit_default_nested.initialized == 29 &&
                   implicit_default_array.initialized == 31 &&
                   implicit_default_base.initialized == 37 &&
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
                    runtime_forwarded_aggregate_source.first == 5 &&
                    runtime_forwarded_aggregate_alias.nested.observed == 3 &&
                    runtime_constructor_array_source[0] == 5 &&
                    runtime_constructor_array_alias.observed == 3 &&
                    runtime_constructor_union_source.value == 5 &&
                    runtime_constructor_union_alias.observed == 3 &&
                   runtime_function_source == 9 &&
                   runtime_function_result == 9 &&
                   runtime_function_aggregate.first == 11 &&
                   runtime_function_aggregate_result == 11 &&
                    runtime_aggregate_object.first == 7 &&
                    runtime_aggregate_object_result == 708 &&
                    runtime_array_source[0] == 7 &&
                    runtime_array_result == 708 &&
                    runtime_union_source.value == 6 &&
                    runtime_union_result == 6 &&
                    runtime_union_switch_source.other == 7 &&
                    runtime_union_switch_result == 7 &&
                    runtime_union_return_source.value == 7 &&
                    runtime_union_return_result.first == 7 &&
                    runtime_union_return_result.second == 8 &&
                    runtime_union_assignment_target.value == 7 &&
                    runtime_union_assignment_result == 7 &&
                    runtime_union_aggregate_source.pair.first == 3 &&
                    runtime_union_aggregate_source.pair.second == 5 &&
                    runtime_union_aggregate_result == 305 &&
                    runtime_union_transition_source.pair.first == 8 &&
                    runtime_union_transition_source.pair.second == 10 &&
                    runtime_union_transition_result == 810 &&
                   runtime_aggregate_return_source == 7 &&
                   runtime_aggregate_return_result.first == 7 &&
                   runtime_aggregate_return_result.second == 8 &&
                    runtime_aggregate_return_object_source.first == 7 &&
                    runtime_aggregate_return_object_result.first == 7 &&
                    runtime_aggregate_return_object_result.second == 8 &&
                    runtime_aggregate_return_array_source[1] == 12 &&
                    runtime_aggregate_return_array_result.first == 5 &&
                    runtime_aggregate_return_array_result.second == 12 &&
                    runtime_assignment_target.first == 6 &&
                    runtime_assignment_target.second == 10 &&
                    runtime_assignment_result == 610 &&
                   runtime_pointer_pair.first == 2 &&
                   runtime_pointer_pair.second == 7 &&
                   runtime_pointer_target == runtime_pointer_replacement &&
                   runtime_pointer_result == 7 &&
                    runtime_aggregate_pointer_original.first == 1 &&
                    runtime_aggregate_pointer_replacement.first == 7 &&
                    runtime_aggregate_pointer_target ==
                        runtime_aggregate_pointer_next &&
                    runtime_aggregate_pointer_result == 708 &&
                    runtime_const_pointer_original == 3 &&
                    runtime_const_pointer_replacement == 9 &&
                    runtime_const_pointer_target == runtime_const_pointer_next &&
                    runtime_const_pointer_result == 9 &&
                    runtime_double_pointer_original == 3 &&
                    runtime_double_pointer_replacement == 10 &&
                    runtime_double_pointer_target == runtime_double_pointer_next &&
                    runtime_double_pointer_result == 10 &&
                   float_first_value == 1.25f &&
                   float_returned_value == 3.75 &&
                   consteval_float_value == 6.25 &&
                   mutated_pair_value == 56 && mutated_array_value == 18
               ? 0 : 1;
}

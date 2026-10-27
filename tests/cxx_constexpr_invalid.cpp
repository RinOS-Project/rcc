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

constexpr int invalid_constexpr_heap_leak() {
    int* value = new int(1);
    return 0;
}

constexpr int invalid_dynamic_leak = invalid_constexpr_heap_leak();

constexpr int invalid_constexpr_array_read() {
    int* values = new int[2];
    return values[0];
}

constexpr int invalid_dynamic_array = invalid_constexpr_array_read();

struct UninitializedConstexprMember {
    int value;

    constexpr UninitializedConstexprMember() {}
};

constexpr UninitializedConstexprMember
    invalid_uninitialized_constexpr_member{};

struct UninitializedNestedAggregate {
    int first;
    int second;
};

struct UninitializedNestedAggregateMember {
    UninitializedNestedAggregate nested;
    int initialized;

    constexpr explicit UninitializedNestedAggregateMember(int value)
        : initialized(value) {}
};

struct UninitializedArrayMember {
    int values[2];
    int initialized;

    constexpr explicit UninitializedArrayMember(int value)
        : initialized(value) {}
};

struct UninitializedBase {
    int value;
};

struct UninitializedBaseSubobject : UninitializedBase {
    int initialized;

    constexpr explicit UninitializedBaseSubobject(int value)
        : initialized(value) {}
};

struct UninitializedBodyMember {
    int written_in_body;
    int omitted;

    constexpr explicit UninitializedBodyMember(int value)
        : written_in_body(value) {
        this->written_in_body = value + 1;
    }
};

struct UninitializedBodyNestedMember {
    UninitializedNestedAggregate nested;
    int written_in_body;

    constexpr explicit UninitializedBodyNestedMember(int value)
        : written_in_body(value) {
        this->written_in_body = value + 2;
    }
};

struct UninitializedBodyMemberRead {
    int omitted;
    int written_in_body;

    constexpr explicit UninitializedBodyMemberRead(int value)
        : written_in_body(value) {
        this->written_in_body = this->omitted++ + value;
    }
};

struct UninitializedCompoundBodyMemberRead {
    int omitted;
    int initialized;

    constexpr explicit UninitializedCompoundBodyMemberRead(int value)
        : initialized(value) {
        this->omitted += value;
    }
};

struct UninitializedBodyStandaloneIncrement {
    int omitted;

    constexpr UninitializedBodyStandaloneIncrement() {
        this->omitted++;
    }
};

struct UninitializedConditionalBodyRead {
    int delayed;
    int selected;

    constexpr explicit UninitializedConditionalBodyRead(bool choose) {
        this->selected = choose ? 1 : this->delayed;
        this->delayed = 2;
    }
};

struct UninitializedBodyReadBeforeWrite {
    int source;
    int target;

    constexpr explicit UninitializedBodyReadBeforeWrite(int value) {
        this->target = this->source + value;
        this->source = value;
    }
};

constexpr UninitializedNestedAggregateMember
    invalid_uninitialized_nested_aggregate_member{3};
constexpr UninitializedArrayMember invalid_uninitialized_array_member{5};
constexpr UninitializedBaseSubobject invalid_uninitialized_base_subobject{7};
constexpr UninitializedBodyMember invalid_uninitialized_body_member{11};
constexpr UninitializedBodyNestedMember
    invalid_uninitialized_body_nested_member{13};
constexpr UninitializedBodyMemberRead
    invalid_uninitialized_body_member_read{17};
constexpr UninitializedCompoundBodyMemberRead
    invalid_uninitialized_compound_body_member_read{18};
constexpr UninitializedBodyStandaloneIncrement
    invalid_uninitialized_body_standalone_increment{};
constexpr UninitializedConditionalBodyRead
    invalid_uninitialized_conditional_body_read{false};
constexpr UninitializedBodyReadBeforeWrite
    invalid_uninitialized_body_read_before_write{19};

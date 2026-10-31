struct ReferenceValue {
    int value;
};

struct XvalueSource {
    ReferenceValue* value;

    operator ReferenceValue&&() {
        return static_cast<ReferenceValue&&>(*value);
    }
};

volatile int verified_volatile_reference_value = 70;

volatile int&& return_volatile_global_reference() {
    return static_cast<volatile int&&>(verified_volatile_reference_value);
}

ReferenceValue& return_lvalue_reference(ReferenceValue& value) {
    return value;
}

ReferenceValue&& return_rvalue_reference(ReferenceValue&& value) {
    return static_cast<ReferenceValue&&>(value);
}

volatile int&& return_volatile_rvalue_reference(volatile int&& value) {
    return static_cast<volatile int&&>(value);
}

using UnaryFunctionReference = int(int);

int triple_for_function_reference(int value) {
    return value * 3;
}

UnaryFunctionReference& forward_function_reference(
    UnaryFunctionReference& function) {
    return function;
}

int main() {
    ReferenceValue value{40};
    XvalueSource source{&value};
    ReferenceValue&& alias = source;
    alias.value += 2;
    ReferenceValue& lvalue_result = return_lvalue_reference(value);
    lvalue_result.value += 3;
    if (value.value != 45) {
        return 1;
    }
    ReferenceValue&& rvalue_result = return_rvalue_reference(
        static_cast<ReferenceValue&&>(value));
    rvalue_result.value += 4;
    if (value.value != 49) {
        return 2;
    }
    volatile int&& volatile_global_result =
        return_volatile_global_reference();
    if (&volatile_global_result != &verified_volatile_reference_value) {
        return 3;
    }
    volatile_global_result += 1;
    if (verified_volatile_reference_value != 71) {
        return 4;
    }
    volatile int volatile_value = 60;
    volatile int&& volatile_result = return_volatile_rvalue_reference(
        static_cast<volatile int&&>(volatile_value));
    if (&volatile_result != &volatile_value) {
        return 5;
    }
    volatile_result += 5;
    if (volatile_value != 65) {
        return 6;
    }
    UnaryFunctionReference& function_reference =
        forward_function_reference(triple_for_function_reference);
    return function_reference(14) != 42;
}

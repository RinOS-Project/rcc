struct ReferenceValue {
    int value;
};

struct XvalueSource {
    ReferenceValue* value;

    operator ReferenceValue&&() {
        return static_cast<ReferenceValue&&>(*value);
    }
};

ReferenceValue& return_lvalue_reference(ReferenceValue& value) {
    return value;
}

ReferenceValue&& return_rvalue_reference(ReferenceValue&& value) {
    return static_cast<ReferenceValue&&>(value);
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
    UnaryFunctionReference& function_reference =
        forward_function_reference(triple_for_function_reference);
    return function_reference(14) != 42;
}

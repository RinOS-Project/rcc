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
    return value.value != 49;
}

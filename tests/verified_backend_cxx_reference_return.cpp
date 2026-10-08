struct ReferenceValue {
    int value;
};

struct XvalueSource {
    ReferenceValue* value;

    operator ReferenceValue&&() {
        return static_cast<ReferenceValue&&>(*value);
    }
};

int main() {
    ReferenceValue value{40};
    XvalueSource source{&value};
    ReferenceValue&& alias = source;
    alias.value += 2;
    return value.value != 42;
}

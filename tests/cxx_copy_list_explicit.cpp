struct Value {
    int value;
    Value(int input) : value(input) {}
};

struct DirectOnlyValue {
    int value;
    explicit DirectOnlyValue(int input) : value(input) {}
};

static int accepted_values;

static void accept(Value value) {
    accepted_values += value.value;
}

template <typename T>
void accept_template(T value) {
    accepted_values += value.value;
}

Value make_returned_value();

Value make_returned_value() {
    return {4};
}

struct Aggregate {
    Value value;
};

int main() {
    DirectOnlyValue explicit_direct{7};
    Value direct{6};
    Value copied = {1};
    Value returned = make_returned_value();
    Aggregate nested{{5}};
    accept({2});
    accept_template<Value>({3});
    return explicit_direct.value + direct.value + copied.value +
           returned.value + nested.value.value + accepted_values == 28
        ? 0 : 1;
}

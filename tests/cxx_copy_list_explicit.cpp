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

struct Aggregate {
    Value value;
};

int main() {
    DirectOnlyValue explicit_direct{7};
    Value direct{6};
    Value copied = {1};
    Aggregate nested{{5}};
    accept({2});
    return explicit_direct.value + direct.value + copied.value +
           nested.value.value + accepted_values == 21 ? 0 : 1;
}

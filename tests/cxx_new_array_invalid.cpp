struct ArrayConstructor {
    int value;

    ArrayConstructor(int first, int second) : value(first + second) {}
};

int invalid_array_new() {
    ArrayConstructor* values = new ArrayConstructor[2]();
    return values[0].value;
}

struct PartialArrayConstructor {
    int value;

    explicit PartialArrayConstructor(int input) : value(input) {}
    PartialArrayConstructor(int first, int second) : value(first + second) {}
};

int invalid_partial_array_new() {
    PartialArrayConstructor* values =
        new PartialArrayConstructor[3]{1};
    return values[0].value;
}

int invalid_negative_array_bound() {
    int* values = new int[-1];
    return values[0];
}

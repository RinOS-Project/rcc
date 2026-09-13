struct ArrayConstructor {
    int value;

    ArrayConstructor() : value(7) {}
};

int invalid_array_new() {
    ArrayConstructor* values = new ArrayConstructor[2]();
    return values[0].value;
}

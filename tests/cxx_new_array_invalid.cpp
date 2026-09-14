struct ArrayConstructor {
    int value;

    ArrayConstructor(int first, int second) : value(first + second) {}
};

int invalid_array_new() {
    ArrayConstructor* values = new ArrayConstructor[2]();
    return values[0].value;
}

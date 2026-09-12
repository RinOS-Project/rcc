int invalid_constructor_new() {
    int* value = new int(42);
    return *value;
}

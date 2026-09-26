int standard_mode_structured_binding() {
    int values[2] = {1, 2};
    auto [first, second] = values;
    return first + second;
}

extern "C" int invalid_forward_reference(void) {
    int values[1] = { 1 };
    for (const auto&& value : values) {
        (void)value;
    }
    return 0;
}

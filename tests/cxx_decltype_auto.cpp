extern "C" decltype(auto) decltype_auto_reference(int& value) {
    return value;
}

extern "C" decltype(auto) decltype_auto_value(void) {
    int value = 41;
    return value + 1;
}

extern "C" auto auto_local_value(void) {
    int value = 40;
    return value + 2;
}

extern "C" decltype(auto) decltype_auto_reference(int& value) {
    return value;
}

extern "C" decltype(auto) decltype_auto_value(void) {
    return 41 + 1;
}

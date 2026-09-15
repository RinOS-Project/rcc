extern "C" int invalid_decltype(void) {
    int value = 1;
    decltype(value + 1) copy = value;
    return copy;
}

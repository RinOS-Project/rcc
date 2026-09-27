static int& decltype_auto_local_ref_source(int& value) {
    return value;
}

extern "C" int decltype_auto_local_probe(void) {
    int value = 40;
    decltype(auto) copy = value;
    decltype(auto) reference = decltype_auto_local_ref_source(value);
    reference += 2;
    return copy + value;
}

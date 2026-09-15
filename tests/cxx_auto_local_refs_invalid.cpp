extern "C" int auto_local_invalid(void) {
    auto& reference = 1;
    return reference;
}

extern "C" int auto_local_pointer_invalid(void) {
    auto* pointer = 1;
    return pointer != 0;
}

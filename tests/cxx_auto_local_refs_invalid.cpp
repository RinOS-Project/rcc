extern "C" int auto_local_invalid(void) {
    auto& reference = 1;
    return reference;
}

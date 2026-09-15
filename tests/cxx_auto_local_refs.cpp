extern "C" int auto_local_reference_probe(void) {
    int value = 7;
    auto& reference = value;
    const auto& const_reference = value;
    auto&& forwarding_reference = value;
    reference = 8;
    return const_reference + forwarding_reference;
}

extern "C" int auto_local_const_probe(void) {
    const auto value = 41;
    return value + 1;
}

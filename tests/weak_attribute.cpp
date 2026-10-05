[[gnu::weak]] extern "C" int rcc_weak_attribute_cpp_function() {
    return 31;
}

extern "C" __attribute__((weak)) int rcc_weak_attribute_cpp_gnu_function() {
    return 32;
}

[[gnu::weak]] extern "C" int rcc_weak_attribute_cpp_data = 33;
